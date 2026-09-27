#pragma once

// Vlastná správa WiFi a konfiguračného AP pre ESP32-S3 (náhrada AutoConnect).
//
// Jediný majiteľ WiFi stavu: STA pripojenie, softAP, captive DNS a captive
// probe URL sú tu. main.cpp iba rozhoduje, kedy portál otvoriť, a obsluhuje
// vlastné /setup stránky.
//
// Zásady pre stabilné AP (telefón sa neodpája):
//  - STA sa pripája iba raz, blokujúco, PRED spustením AP. Ak sa nepripojí,
//    v AP režime sa STA nepokúša znovu (žiadne skeny = žiadne skoky kanálov),
//    okrem explicitne povoleného pomalého retry (recovery mód).
//  - AP beží na pevnom kanáli; keď je STA pripojené, IDF si kanál zladí sám
//    pri štarte AP, nie neskôr.
//  - Modem sleep je počas AP vypnutý (inak sú odpovede portálu pomalé).

#include <Arduino.h>
#include <FS.h>
#include <IPAddress.h>
#include <WebServer.h>
#include <functional>

struct WifiCredentials {
  char ssid[33]{0};
  char password[65]{0};
  bool valid{false};
};

namespace WifiPortal {

// /wifi.json: {"ssid":"...","password":"..."} — zapisuje ho aj web flasher.
static constexpr const char *kCredentialsFile = "/wifi.json";

// Načíta credentials z /wifi.json. Ak súbor chýba, skúsi jednorazovo
// migrovať posledný záznam z NVS úložiska AutoConnect (staršie firmvéry)
// a uloží ho do /wifi.json.
bool loadCredentials(fs::FS &fs, WifiCredentials &out);
bool saveCredentials(fs::FS &fs, const char *ssid, const char *password);

// Blokujúce pripojenie k STA (max. timeoutMs). whileWaiting sa volá
// opakovane počas čakania (napr. lv_task_handler()).
bool connectSta(const WifiCredentials &creds, unsigned long timeoutMs,
                const std::function<void()> &whileWaiting);

// Spustí konfiguračné AP (+ captive DNS). STA zostáva ako je: pripojené,
// alebo nečinné bez opakovaných pokusov.
void startAp(const String &ssid, const String &psk);
void stopAp();
bool apActive();
IPAddress apIp();
const String &apSsid();

// Pomalé opakovanie STA pripojenia počas AP (recovery: WiFi je nutné, ale
// nedostupné). Každý pokus krátko rozhodí AP, preto interval v minútach.
void setBackgroundStaRetry(bool enabled, unsigned long intervalMs);

// Volať z loop(): captive DNS, STA watchdog.
void loop();

// true, keď požiadavka prišla z AP podsiete (192.168.4.x).
bool isApClient(WebServer &server);

// Captive probe URL (Android/iOS/Windows) + onNotFound: AP klientov
// presmeruje absolútnou URL na portalPath, STA klientom odpovie "je internet".
void registerCaptiveRoutes(WebServer &server, const char *portalPath);

} // namespace WifiPortal
