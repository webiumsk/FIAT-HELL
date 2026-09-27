#include "WifiPortal.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>

namespace WifiPortal {

namespace {

const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApNetmask(255, 255, 255, 0);
constexpr uint8_t kApChannel = 6;
constexpr uint8_t kApMaxClients = 4;

String g_apSsid;
String g_apPsk;
bool g_apActive = false;

WifiCredentials g_creds; // posledné použité STA credentials (pre watchdog)
unsigned long g_lastStaAttempt = 0;
bool g_bgRetryEnabled = false;
unsigned long g_bgRetryInterval = 5UL * 60UL * 1000UL;
constexpr unsigned long kStaWatchdogInterval = 30UL * 1000UL;

// ---------------------------------------------------------------------------
// Captive DNS: každý dotaz zodpovieme IP adresou AP.
//
// Arduino DNSServer kazí EDNS0 dotazy (moderné telefóny ich posielajú) na
// nevalidné odpovede, preto minimálny vlastný responder: zahodí additional
// sekciu a vráti platnú A odpoveď; na iné typy vráti platnú prázdnu odpoveď.
// ---------------------------------------------------------------------------
WiFiUDP g_dnsUdp;
bool g_dnsRunning = false;

void dnsStart() {
  if (g_dnsRunning) return;
  if (g_dnsUdp.begin(53)) {
    g_dnsRunning = true;
    Serial.println("Captive DNS up -> " + kApIp.toString());
  }
}

void dnsStop() {
  if (!g_dnsRunning) return;
  g_dnsUdp.stop();
  g_dnsRunning = false;
}

bool dnsProcessOnce() {
  const int avail = g_dnsUdp.parsePacket();
  if (avail <= 0) return false;

  static uint8_t buf[512];
  int n = g_dnsUdp.read(buf, sizeof(buf));
  if (n < 12) return true;        // príliš krátke na dotaz
  if (buf[2] & 0x80) return true; // už je to odpoveď

  // Prvá otázka: QNAME (labely), potom QTYPE(2) + QCLASS(2).
  int p = 12;
  bool compressed = false;
  while (p < n && buf[p] != 0) {
    if ((buf[p] & 0xC0) == 0xC0) {
      p += 2;
      compressed = true;
      break;
    }
    p += buf[p] + 1;
  }
  if (p >= n) return true;
  if (!compressed) p += 1; // nulový label
  if (p + 4 > n) return true;
  const uint16_t qtype = (buf[p] << 8) | buf[p + 1];
  const int qend = p + 4;

  // Hlavička odpovede: QR=1, AA=1, RD zachovať; RCODE=0; NS/AR=0.
  buf[2] = 0x84 | (buf[2] & 0x01);
  buf[3] = 0x00;
  buf[6] = 0x00; buf[8] = 0x00; buf[10] = 0x00; buf[11] = 0x00;
  const bool answerA = (qtype == 1);
  buf[7] = answerA ? 0x01 : 0x00; // ANCOUNT

  int outLen = qend;
  if (answerA && qend + 16 <= (int)sizeof(buf)) {
    uint8_t *a = buf + qend;
    a[0] = 0xC0; a[1] = 0x0C;                 // meno -> pointer na otázku
    a[2] = 0x00; a[3] = 0x01;                 // TYPE A
    a[4] = 0x00; a[5] = 0x01;                 // CLASS IN
    a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 30;  // TTL 30 s
    a[10] = 0x00; a[11] = 0x04;               // RDLENGTH
    a[12] = kApIp[0]; a[13] = kApIp[1]; a[14] = kApIp[2]; a[15] = kApIp[3];
    outLen = qend + 16;
  }

  g_dnsUdp.beginPacket(g_dnsUdp.remoteIP(), g_dnsUdp.remotePort());
  g_dnsUdp.write(buf, outLen);
  g_dnsUdp.endPacket();
  return true;
}

void dnsProcess() {
  for (int i = 0; i < 6 && dnsProcessOnce(); i++) {
  }
}

// ---------------------------------------------------------------------------
// Migrácia z AutoConnect NVS (namespace "AC_CREDT", kľúč "AC_CREDT").
// Formát: |e|ss(lo,hi)|ssid\0pass\0bssid(6)|dhcp|[ip gw nm dns1 dns2]|...
// Posledný záznam je naposledy uložený (AutoConnect save() maže a pridáva).
// ---------------------------------------------------------------------------
bool importAutoConnectCredentials(WifiCredentials &out) {
  Preferences pref;
  if (!pref.begin("AC_CREDT", true)) return false;
  const size_t psz = pref.getBytesLength("AC_CREDT");
  if (psz < 4 || psz > 4096) {
    pref.end();
    return false;
  }
  uint8_t *pool = (uint8_t *)malloc(psz);
  if (!pool) {
    pref.end();
    return false;
  }
  pref.getBytes("AC_CREDT", pool, psz);
  pref.end();

  bool found = false;
  size_t dp = 3; // preskočiť e + ss
  while (dp + 1 < psz) {
    if (pool[dp] == 0) break; // koniec kontajnera
    const char *ssid = reinterpret_cast<const char *>(pool + dp);
    const size_t ssidLen = strnlen(ssid, psz - dp);
    dp += ssidLen + 1;
    if (dp >= psz) break;
    const char *pass = reinterpret_cast<const char *>(pool + dp);
    const size_t passLen = strnlen(pass, psz - dp);
    dp += passLen + 1;
    if (dp + 6 + 1 > psz) break;
    dp += 6; // bssid
    const uint8_t dhcp = pool[dp++];
    if (dhcp == 1) dp += 5 * 4; // STA_STATIC: ip, gw, nm, dns1, dns2
    if (ssidLen > 0 && ssidLen < sizeof(out.ssid) &&
        passLen < sizeof(out.password)) {
      strlcpy(out.ssid, ssid, sizeof(out.ssid));
      strlcpy(out.password, pass, sizeof(out.password));
      out.valid = true;
      found = true;
    }
  }
  free(pool);
  return found;
}

void applyStaConfigFrom(const WifiCredentials &creds) {
  g_creds = creds;
}

} // namespace

// ---------------------------------------------------------------------------
// Credentials
// ---------------------------------------------------------------------------
bool loadCredentials(fs::FS &fs, WifiCredentials &out) {
  out = WifiCredentials();
  File f = fs.open(kCredentialsFile, "r");
  if (f) {
    DynamicJsonDocument doc(512);
    const bool ok = (deserializeJson(doc, f) == DeserializationError::Ok);
    f.close();
    if (ok) {
      const char *ssid = doc["ssid"] | "";
      const char *pwd = doc["password"] | "";
      if (ssid[0] != '\0') {
        strlcpy(out.ssid, ssid, sizeof(out.ssid));
        strlcpy(out.password, pwd, sizeof(out.password));
        out.valid = true;
        return true;
      }
    }
    // súbor existuje, ale je prázdny/nevalidný -> žiadne credentials
    return false;
  }

  if (importAutoConnectCredentials(out)) {
    Serial.print("WiFi: migrated AutoConnect credentials for ");
    Serial.println(out.ssid);
    saveCredentials(fs, out.ssid, out.password);
    return true;
  }
  return false;
}

bool saveCredentials(fs::FS &fs, const char *ssid, const char *password) {
  DynamicJsonDocument doc(512);
  doc["ssid"] = ssid ? ssid : "";
  doc["password"] = password ? password : "";
  File f = fs.open(kCredentialsFile, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ---------------------------------------------------------------------------
// STA
// ---------------------------------------------------------------------------
bool connectSta(const WifiCredentials &creds, unsigned long timeoutMs,
                const std::function<void()> &whileWaiting) {
  if (!creds.valid) return false;
  applyStaConfigFrom(creds);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(g_apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname("fiathell");
  WiFi.begin(creds.ssid, creds.password);
  g_lastStaAttempt = millis();

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    if (whileWaiting) whileWaiting();
    delay(20);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Bežný STA režim: nech IDF drží spojenie sám.
    WiFi.setAutoReconnect(true);
    return true;
  }

  // Neúspech: nechať STA nečinné. Bez tohto by IDF skenoval dokola a v AP
  // režime by telefóny padali kvôli skokom kanálov.
  WiFi.disconnect(false, false);
  return false;
}

void setBackgroundStaRetry(bool enabled, unsigned long intervalMs) {
  g_bgRetryEnabled = enabled;
  if (intervalMs > 0) g_bgRetryInterval = intervalMs;
}

// ---------------------------------------------------------------------------
// AP
// ---------------------------------------------------------------------------
void startAp(const String &ssid, const String &psk) {
  if (g_apActive) return;
  g_apSsid = ssid;
  g_apPsk = psk;

  WiFi.persistent(false);
  WiFi.setSleep(false); // modem sleep spomaľuje odpovede softAP

  const bool staConnected = (WiFi.status() == WL_CONNECTED);
  WiFi.mode(WIFI_AP_STA);
  if (!staConnected) {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
  }

  WiFi.softAPConfig(kApIp, kApGateway, kApNetmask);
  // Keď je STA pripojené, IDF si kanál AP zladí so STA (jedno rádio).
  const bool ok = WiFi.softAP(g_apSsid.c_str(),
                              g_apPsk.length() >= 8 ? g_apPsk.c_str() : nullptr,
                              kApChannel, 0, kApMaxClients);
  delay(100);
  // softAPConfig ešte raz po štarte — na niektorých verziách jadra sa
  // nastavenie pred softAP() neuplatní.
  WiFi.softAPConfig(kApIp, kApGateway, kApNetmask);

  g_apActive = ok;
  if (ok) {
    dnsStart();
    Serial.println("Config AP up: " + g_apSsid + " -> " +
                   WiFi.softAPIP().toString() +
                   (staConnected ? " (STA connected)" : " (STA idle)"));
  } else {
    Serial.println("Config AP failed to start");
  }
}

void stopAp() {
  if (!g_apActive) return;
  dnsStop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  g_apActive = false;
  g_bgRetryEnabled = false;
  if (g_creds.valid) WiFi.setAutoReconnect(true);
  Serial.println("Config AP closed");
}

bool apActive() { return g_apActive; }
IPAddress apIp() { return kApIp; }
const String &apSsid() { return g_apSsid; }

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  if (g_apActive) {
    dnsProcess();
    // Recovery: pomalé opakovanie STA (každý pokus krátko rozhodí AP).
    if (g_bgRetryEnabled && g_creds.valid &&
        WiFi.status() != WL_CONNECTED &&
        millis() - g_lastStaAttempt >= g_bgRetryInterval) {
      g_lastStaAttempt = millis();
      Serial.println("WiFi: background STA retry");
      WiFi.begin(g_creds.ssid, g_creds.password);
    }
    return;
  }

  // Bežný STA režim: IDF autoReconnect + vlastný watchdog pre istotu.
  if (g_creds.valid && WiFi.status() != WL_CONNECTED &&
      millis() - g_lastStaAttempt >= kStaWatchdogInterval) {
    g_lastStaAttempt = millis();
    Serial.println("WiFi: watchdog reconnect");
    WiFi.begin(g_creds.ssid, g_creds.password);
  }
}

// ---------------------------------------------------------------------------
// HTTP: captive probes
// ---------------------------------------------------------------------------
bool isApClient(WebServer &server) {
  const IPAddress c = server.client().remoteIP();
  return c[0] == 192 && c[1] == 168 && c[2] == 4;
}

void registerCaptiveRoutes(WebServer &server, const char *portalPath) {
  static String s_portalUrl;
  s_portalUrl = "http://" + kApIp.toString() + String(portalPath);

  // Absolútna URL v Location je to, čo telefóny rozpoznajú ako captive
  // portál a automaticky otvoria prihlasovaciu stránku.
  auto redirectToPortal = [&server]() {
    server.sendHeader("Location", s_portalUrl, true);
    server.sendHeader("Cache-Control", "no-store");
    server.send(302, "text/plain", "");
    server.client().stop();
  };

  // Android
  auto android = [&server, redirectToPortal]() {
    if (isApClient(server)) { redirectToPortal(); return; }
    server.send(204, "text/plain", "");
  };
  server.on("/generate_204", android);
  server.on("/gen_204", android);

  // Apple (iOS / macOS CNA)
  auto apple = [&server, redirectToPortal]() {
    if (isApClient(server)) { redirectToPortal(); return; }
    server.send(200, "text/html",
                "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  };
  server.on("/hotspot-detect.html", apple);
  server.on("/library/test/success.html", apple);

  // Windows NCSI
  auto windows = [&server, redirectToPortal]() {
    if (isApClient(server)) { redirectToPortal(); return; }
    server.send(200, "text/plain", "Microsoft Connect Test");
  };
  server.on("/connecttest.txt", windows);
  server.on("/ncsi.txt", windows);
  server.on("/fwlink", windows);
  server.on("/redirect", windows);

  // Firefox / iné
  auto generic = [&server, redirectToPortal]() {
    if (isApClient(server)) { redirectToPortal(); return; }
    server.send(200, "text/plain", "success");
  };
  server.on("/success.txt", generic);
  server.on("/canonical.html", generic);

  // Všetko ostatné z AP -> portál; zo STA siete -> 404.
  server.onNotFound([&server, redirectToPortal]() {
    if (isApClient(server)) { redirectToPortal(); return; }
    server.send(404, "text/plain", "");
  });
}

} // namespace WifiPortal
