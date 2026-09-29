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
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Flash API key</title>%%STYLE%%</head><body>
<h1>&#9889; Get a Flash API key</h1>
%%OFFLINE_WARN%%
<ol>
  <li>Install the <b>Flash</b> app and register with your phone number (if you don't have an account yet).</li>
  <li>In the app, request an <b>SMS code</b> for your number on the login screen.</li>
  <li><b>Don't enter the code in the app</b> &mdash; enter it below instead. The code is valid for only a few minutes.</li>
</ol>
<form method="POST" action="/flashkey/run">
  <label>Phone number (international format)</label>
  <input type="tel" name="phone" placeholder="+15551234567" required>
  <label>6-digit SMS code</label>
  <input type="text" name="code" placeholder="123456" minlength="6" maxlength="6"
         inputmode="numeric" required>
  <button type="submit">Create and save key</button>
</form>
<p><a href="/">&larr; back</a></p>
</body></html>)";

inline String flashKeyPageHtml(bool online) {
  String html = FPSTR(FLASHKEY_PAGE_HTML);
  html.replace(F("%%STYLE%%"), FPSTR(FLASHKEY_STYLE));
  html.replace(F("%%OFFLINE_WARN%%"),
               online ? ""
                      : "<div class=warn>&#9888; The device is not connected "
                        "to the internet &mdash; set up WiFi first, otherwise "
                        "key creation will fail.</div>");
  return html;
}

// Escape text before embedding it in HTML — detail may carry error messages
// straight from the Flash API and the API key itself.
inline String flashKeyEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    const char c = in[i];
    switch (c) {
    case '&': out += F("&amp;"); break;
    case '<': out += F("&lt;"); break;
    case '>': out += F("&gt;"); break;
    case '"': out += F("&quot;"); break;
    case '\'': out += F("&#39;"); break;
    default: out += c;
    }
  }
  return out;
}

// `detail` is always treated as plain text and escaped. `keyToShow`, when
// non-empty, is rendered (escaped) in the styled key box.
inline String flashKeyResultHtml(bool ok, const String &detail,
                                 const String &keyToShow = String()) {
  String html = F("<!DOCTYPE html><html lang=\"en\"><head><meta "
                  "charset=\"utf-8\"><meta name=\"viewport\" "
                  "content=\"width=device-width,initial-scale=1\">"
                  "<title>Flash API key</title>%%STYLE%%</head><body>"
                  "<h1>&#9889; Flash API key</h1>");
  html.replace(F("%%STYLE%%"), FPSTR(FLASHKEY_STYLE));
  if (ok) {
    html += F("<div class=ok>&#10004; The key was created and stored on the "
              "device. The ATM is ready to pay out via Flash.</div>"
              "<p>This is your key &mdash; it is shown <b>only once</b>. "
              "Store it somewhere safe (e.g. a password manager):</p>"
              "<div class=key>");
    html += flashKeyEscape(detail);
    html += F("</div><p>No wallet ID needed &mdash; the device discovers it "
              "by itself.</p>");
  } else {
    html += F("<div class=err>&#10006; Failed: ");
    html += flashKeyEscape(detail);
    html += F("</div>");
    if (keyToShow.length() > 0) {
      html += F("<div class=key>");
      html += flashKeyEscape(keyToShow);
      html += F("</div>");
    }
    html += F("<p>Request a new SMS code in the app and <a "
              "href=\"/flashkey\">try again</a>.</p>");
  }
  html += F("<p><a href=\"/\">&larr; back</a></p></body></html>");
  return html;
}

/**
 * Execute the wizard: login with phone+code, create the API key, persist it
 * into /first.json and DeviceState. Returns the result page HTML.
 */
inline String flashKeyRunAndRender(DeviceState &ds,
                                   ConfigService &configService, fs::FS &fs,
                                   const char *firstFile, String phone,
                                   String code) {
  phone.trim();
  code.trim();
  if (phone.length() < 8 || phone[0] != '+') {
    return flashKeyResultHtml(
        false, F("the number must be in international format (+1...)"));
  }
  if (code.length() != 6) {
    return flashKeyResultHtml(false, F("the code must have 6 digits"));
  }

  String authToken, apiKey, err;
  if (!FlashAuthService::userLogin(phone, code, authToken, err)) {
    return flashKeyResultHtml(false, "login failed: " + err);
  }
  if (!FlashAuthService::apiKeyCreate(authToken, apiKey, err)) {
    return flashKeyResultHtml(false, "key creation failed: " + err);
  }

  if (!configService.updateFirstBlinkApiKey(fs, firstFile, apiKey.c_str())) {
    // Key exists on the account but we couldn't persist it - show it so the
    // operator can enter it manually instead of losing it forever.
    return flashKeyResultHtml(
        false,
        "the key was created but saving it to the device failed - store it "
        "and enter it manually in the portal:",
        apiKey);
  }
  strlcpy(ds.blinkapikey, apiKey.c_str(), sizeof(ds.blinkapikey));
  Serial.println("flashkey: API key created and saved");
  return flashKeyResultHtml(true, apiKey);
}
