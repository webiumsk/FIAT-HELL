#include "services/SerialConfigService.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

// Secrets never leave the device over READ_CONFIG: their values are replaced
// by this marker so the host only learns that something is configured.
#define SECRET_SET_MARKER "__SET__"
// The host sends this marker for secrets it wants to leave untouched; the
// device substitutes the currently stored value while writing.
#define SECRET_KEEP_MARKER "__KEEP__"

namespace SerialConfigService {

// Scan visible networks and print them as one JSON line the flasher can
// parse into an SSID dropdown. Deduped by SSID, strongest first, capped.
static void scanAndPrintWifiList() {
  WiFi.mode(WIFI_STA);
  const int16_t found = WiFi.scanNetworks(/*async=*/false);

  struct Net {
    String ssid;
    int32_t rssi;
    bool secure;
  };
  std::vector<Net> nets;
  for (int16_t i = 0; i < found; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue; // hidden networks
    bool known = false;
    for (auto &n : nets) {
      if (n.ssid == ssid) {
        known = true;
        if (WiFi.RSSI(i) > n.rssi) n.rssi = WiFi.RSSI(i);
        break;
      }
    }
    if (!known) {
      nets.push_back(
          {ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    }
  }
  WiFi.scanDelete();

  std::sort(nets.begin(), nets.end(),
            [](const Net &a, const Net &b) { return a.rssi > b.rssi; });
  if (nets.size() > 20) nets.resize(20);

  String out = "FIAT-HELL:WIFI_LIST:[";
  for (size_t i = 0; i < nets.size(); i++) {
    if (i > 0) out += ',';
    String escaped = nets[i].ssid;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    out += "{\"ssid\":\"" + escaped + "\",\"rssi\":" + String(nets[i].rssi) +
           ",\"secure\":" + (nets[i].secure ? "true" : "false") + "}";
  }
  out += "]";
  Serial.println(out);
}

// ---------------------------------------------------------------------------
// READ_CONFIG (redacted dump) + __KEEP__ merge on WRITE_CONFIG
// ---------------------------------------------------------------------------

static String csvComponent(const String &csv, int index) {
  int start = 0;
  for (int i = 0; i < index; i++) {
    const int comma = csv.indexOf(',', start);
    if (comma < 0) return String("");
    start = comma + 1;
  }
  const int comma = csv.indexOf(',', start);
  return comma < 0 ? csv.substring(start) : csv.substring(start, comma);
}

static String csvReplaceComponent(const String &csv, int index,
                                  const String &newValue) {
  String out;
  int start = 0, i = 0;
  while (true) {
    const int comma = csv.indexOf(',', start);
    const String part =
        comma < 0 ? csv.substring(start) : csv.substring(start, comma);
    if (i > 0) out += ',';
    out += (i == index) ? newValue : part;
    if (comma < 0) break;
    start = comma + 1;
    i++;
  }
  return out;
}

static void maskValue(JsonObject entry) {
  const char *v = entry["value"] | "";
  entry["value"] = (v[0] != '\0') ? SECRET_SET_MARKER : "";
}

static void maskCsvSecret(JsonObject entry, int component) {
  String csv = String((const char *)(entry["value"] | ""));
  if (csvComponent(csv, component).length() > 0) {
    entry["value"] = csvReplaceComponent(csv, component, SECRET_SET_MARKER);
  }
}

// Redact per-file secrets in place. Positional layout matches ConfigService.
static void redactSecrets(const String &path, JsonDocument &doc) {
  if (path == "/elements.json") {
    maskValue(doc[0]); // AP portal password
  } else if (path == "/first.json") {
    maskValue(doc[0]);        // blinkapikey
    maskCsvSecret(doc[2], 1); // lnurl CSV: base,SECRET,currency
    maskValue(doc[3]);        // adminkey
    maskValue(doc[4]);        // readkey
  } else if (path == "/second.json" || path == "/third.json") {
    maskCsvSecret(doc[1], 1); // lnurlN CSV
  } else if (path == "/wifi.json") {
    JsonObject o = doc.as<JsonObject>();
    const char *pwd = o["password"] | "";
    o["password"] = (pwd[0] != '\0') ? SECRET_SET_MARKER : "";
  }
}

static const char *const kConfigFiles[] = {"/elements.json", "/gui.json",
                                           "/first.json",    "/second.json",
                                           "/third.json",    "/wifi.json"};

static bool isAllowedConfigPath(const String &path) {
  for (auto p : kConfigFiles) {
    if (path == p) return true;
  }
  return false;
}

static void dumpRedactedConfig(fs::FS &fs) {
  for (auto path : kConfigFiles) {
    File f = fs.open(path, "r");
    if (!f) continue;
    DynamicJsonDocument doc(2400);
    const bool ok = (deserializeJson(doc, f) == DeserializationError::Ok);
    f.close();
    if (!ok) continue;
    redactSecrets(path, doc);
    String out;
    serializeJson(doc, out);
    Serial.print("FIAT-HELL:CONFIG_FILE:");
    Serial.print(path);
    Serial.print(":");
    Serial.println(out);
  }
  Serial.println("FIAT-HELL:CONFIG_DUMP_DONE");
}

// Replace __KEEP__ markers in an incoming value with the currently stored
// one (or "" when nothing is stored - the marker must never be persisted).
// Values are either plain strings or base,secret,currency CSV triples.
static String resolveKeep(const String &incoming, const String &stored) {
  if (incoming == SECRET_KEEP_MARKER) return stored;
  if (incoming.indexOf(SECRET_KEEP_MARKER) < 0) return incoming;
  String out = incoming;
  for (int i = 0; i < 3; i++) {
    if (csvComponent(out, i) == SECRET_KEEP_MARKER) {
      out = csvReplaceComponent(out, i, csvComponent(stored, i));
    }
  }
  return out;
}

// Merge __KEEP__ markers in fileContent against the existing file. Returns
// the content to write (unchanged when no markers are present).
static String mergeKeepMarkers(fs::FS &fs, const String &path,
                               const String &fileContent) {
  if (fileContent.indexOf(SECRET_KEEP_MARKER) < 0) return fileContent;

  DynamicJsonDocument incoming(2400);
  if (deserializeJson(incoming, fileContent) != DeserializationError::Ok) {
    return fileContent;
  }

  DynamicJsonDocument existing(2400);
  bool haveExisting = false;
  File f = fs.open(path, "r");
  if (f) {
    haveExisting = (deserializeJson(existing, f) == DeserializationError::Ok);
    f.close();
  }

  if (incoming.is<JsonArray>()) {
    JsonArray arr = incoming.as<JsonArray>();
    for (size_t i = 0; i < arr.size(); i++) {
      JsonObject entry = arr[i];
      const char *v = entry["value"] | (const char *)nullptr;
      if (!v || strstr(v, SECRET_KEEP_MARKER) == nullptr) continue;
      String stored;
      if (haveExisting) {
        // Match by name; a matched (even empty) value is authoritative.
        // Only fall back to the same position when NO name matched, so a
        // reordered or unknown incoming name can't inherit an unrelated
        // stored credential.
        const char *name = entry["name"] | "";
        bool nameMatched = false;
        for (JsonObject old : existing.as<JsonArray>()) {
          if (strcmp(old["name"] | "", name) == 0) {
            stored = (const char *)(old["value"] | "");
            nameMatched = true;
            break;
          }
        }
        if (!nameMatched && i < existing.as<JsonArray>().size()) {
          stored = (const char *)(existing[i]["value"] | "");
        }
      }
      entry["value"] = resolveKeep(String(v), stored);
    }
  } else if (incoming.is<JsonObject>()) {
    // wifi.json: only the password field carries a secret
    const char *pwd = incoming["password"] | "";
    if (String(pwd) == SECRET_KEEP_MARKER) {
      incoming["password"] =
          haveExisting ? (const char *)(existing["password"] | "") : "";
    }
  }

  String merged;
  serializeJson(incoming, merged);
  return merged;
}

void runSerialConfigWindow(fs::FS &fs) {
  Serial.println("FIAT-HELL:CONFIG_READY");

  const unsigned long detectEnd = millis() + 2000UL;
  while (millis() < detectEnd && !Serial.available()) yield();
  if (!Serial.available()) return;

  unsigned long cfgDeadline = millis() + 10000UL;
  while (millis() < cfgDeadline) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("WRITE_CONFIG:")) {
        // format: WRITE_CONFIG:/filename.json:{json_content}
        const int colon2 = line.indexOf(':', 13);
        if (colon2 > 13) {
          String filePath = line.substring(13, colon2);
          if (!isAllowedConfigPath(filePath)) {
            // Never let the serial channel write outside the known config set.
            Serial.print("FIAT-HELL:REJECTED:");
            Serial.println(filePath);
          } else {
            String fileContent =
                mergeKeepMarkers(fs, filePath, line.substring(colon2 + 1));
            File f = fs.open(filePath, "w");
            if (f) {
              f.print(fileContent);
              f.close();
            }
            Serial.print("FIAT-HELL:WROTE:");
            Serial.println(filePath);
          }
        }
      } else if (line == "READ_CONFIG") {
        dumpRedactedConfig(fs);
        cfgDeadline = millis() + 10000UL;
      } else if (line == "SCAN_WIFI") {
        scanAndPrintWifiList();
        // The scan takes seconds - give the host a fresh window to react.
        cfgDeadline = millis() + 10000UL;
      } else if (line == "CONFIG_DONE") {
        Serial.println("FIAT-HELL:CONFIG_SAVED");
        break;
      }
    }
    yield();
  }
}

} // namespace SerialConfigService
