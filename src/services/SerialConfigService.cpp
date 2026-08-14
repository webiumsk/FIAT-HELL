#include "services/SerialConfigService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

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
          String fileContent = line.substring(colon2 + 1);
          File f = fs.open(filePath, "w");
          if (f) {
            f.print(fileContent);
            f.close();
          }
          Serial.print("FIAT-HELL:WROTE:");
          Serial.println(filePath);
        }
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
