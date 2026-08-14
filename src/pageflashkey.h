// On-device "get Flash API key" wizard page (/flashkey).
// Shared by both boards; the board main.cpp wires the routes:
//   GET  /flashkey      -> flashKeyPageHtml(wifiStatus())
//   POST /flashkey/run  -> flashKeyRunAndRender(...)
// The SMS code must be requested via the Flash mobile app (the API's
// code-request path is captcha-gated); the device then finishes login +
// apiKeyCreate itself, so the key never leaves the ATM.
#pragma once

#include "DeviceState.h"
#include "services/ConfigService.h"
#include "services/FlashAuthService.h"
#include <Arduino.h>
#include <FS.h>
#include <HTTPClient.h>

static const char FLASHKEY_STYLE[] PROGMEM = R"(<style>
body{background:#111;color:#eee;font-family:Arial,sans-serif;margin:0;padding:16px;max-width:520px;margin:auto}
h1{color:#f90;font-size:1.3em}
ol{color:#bbb;line-height:1.5;padding-left:20px}
label{display:block;color:#999;font-size:.85em;margin-top:12px}
input{display:block;width:100%;padding:10px;margin-top:4px;box-sizing:border-box;
background:#1e1e1e;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em}
button{display:block;width:100%;margin-top:20px;padding:14px;background:#f90;color:#000;
border:none;border-radius:8px;font-size:1.05em;font-weight:bold;cursor:pointer}
.warn{background:#331a00;border:1px solid #f90;color:#f90;padding:10px;border-radius:8px;margin:12px 0}
.err{background:#330000;border:1px solid #f33;color:#f66;padding:10px;border-radius:8px;margin:12px 0}
.ok{background:#0a2a0a;border:1px solid #3c3;color:#6f6;padding:10px;border-radius:8px;margin:12px 0}
.key{word-break:break-all;font-family:monospace;background:#1e1e1e;border:1px dashed #f90;
padding:12px;border-radius:8px;margin:10px 0;font-size:.95em}
a{color:#f90}
</style>)";

static const char FLASHKEY_PAGE_HTML[] PROGMEM = R"(<!DOCTYPE html>
<html lang="sk"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Flash API kluc</title>%%STYLE%%</head><body>
<h1>&#9889; Ziskat Flash API kluc</h1>
%%OFFLINE_WARN%%
<ol>
  <li>Nainstaluj si appku <b>Flash</b> a zaregistruj sa svojim telefonnym cislom (ak este nemas ucet).</li>
  <li>V appke si na prihlasovacej obrazovke vyziadaj <b>SMS kod</b> pre svoje cislo.</li>
  <li><b>Kod nezadavaj do appky</b> &mdash; zadaj ho sem dole. Kod plati len par minut.</li>
</ol>
<form method="POST" action="/flashkey/run">
  <label>Telefonne cislo (medzinarodny format)</label>
  <input type="tel" name="phone" placeholder="+421900123456" required>
  <label>6-miestny SMS kod</label>
  <input type="text" name="code" placeholder="123456" minlength="6" maxlength="6"
         inputmode="numeric" required>
  <button type="submit">Vytvorit a ulozit kluc</button>
</form>
<p><a href="/">&larr; spat</a></p>
</body></html>)";

inline String flashKeyPageHtml(bool online) {
  String html = FPSTR(FLASHKEY_PAGE_HTML);
  html.replace(F("%%STYLE%%"), FPSTR(FLASHKEY_STYLE));
  html.replace(F("%%OFFLINE_WARN%%"),
               online ? ""
                      : "<div class=warn>&#9888; Zariadenie nie je pripojene "
                        "na internet &mdash; najprv nastav WiFi, inak "
                        "vytvorenie kluca zlyha.</div>");
  return html;
}

inline String flashKeyResultHtml(bool ok, const String &detail) {
  String html = F("<!DOCTYPE html><html lang=\"sk\"><head><meta "
                  "charset=\"utf-8\"><meta name=\"viewport\" "
                  "content=\"width=device-width,initial-scale=1\">"
                  "<title>Flash API kluc</title>%%STYLE%%</head><body>"
                  "<h1>&#9889; Flash API kluc</h1>");
  html.replace(F("%%STYLE%%"), FPSTR(FLASHKEY_STYLE));
  if (ok) {
    html += F("<div class=ok>&#10004; Kluc bol vytvoreny a ulozeny do "
              "zariadenia. ATM je pripravene vyplacat cez Flash.</div>"
              "<p>Toto je tvoj kluc &mdash; zobrazuje sa <b>len raz</b>. "
              "Odloz si ho na bezpecne miesto (napr. spravca hesiel):</p>"
              "<div class=key>");
    html += detail;
    html += F("</div><p>Wallet ID netreba &mdash; zariadenie si ho zisti "
              "samo.</p>");
  } else {
    html += F("<div class=err>&#10006; Nepodarilo sa: ");
    html += detail;
    html += F("</div><p>Vyziadaj si v appke novy SMS kod a <a "
              "href=\"/flashkey\">skus znova</a>.</p>");
  }
  html += F("<p><a href=\"/\">&larr; spat</a></p></body></html>");
  return html;
}

/**
 * Execute the wizard: login with phone+code, create the API key, persist it
 * into /first.json and DeviceState. Returns the result page HTML.
 */
inline String flashKeyRunAndRender(HTTPClient &http, DeviceState &ds,
                                   ConfigService &configService, fs::FS &fs,
                                   const char *firstFile, String phone,
                                   String code) {
  phone.trim();
  code.trim();
  if (phone.length() < 8 || phone[0] != '+') {
    return flashKeyResultHtml(
        false, F("cislo musi byt v medzinarodnom formate (+421...)"));
  }
  if (code.length() != 6) {
    return flashKeyResultHtml(false, F("kod musi mat 6 cislic"));
  }

  String authToken, apiKey, err;
  if (!FlashAuthService::userLogin(http, phone, code, authToken, err)) {
    return flashKeyResultHtml(false, "prihlasenie zlyhalo: " + err);
  }
  if (!FlashAuthService::apiKeyCreate(http, authToken, apiKey, err)) {
    return flashKeyResultHtml(false, "vytvorenie kluca zlyhalo: " + err);
  }

  if (!configService.updateFirstBlinkApiKey(fs, firstFile, apiKey.c_str())) {
    // Key exists on the account but we couldn't persist it - show it so the
    // operator can enter it manually instead of losing it forever.
    return flashKeyResultHtml(
        false, "kluc vznikol, ale zapis do zariadenia zlyhal - odloz si ho a "
               "zadaj rucne v portali: <div class=key>" + apiKey + "</div>");
  }
  strlcpy(ds.blinkapikey, apiKey.c_str(), sizeof(ds.blinkapikey));
  Serial.println("flashkey: API key created and saved");
  return flashKeyResultHtml(true, apiKey);
}
