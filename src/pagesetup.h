#pragma once

// Mobile-friendly config portal page served at /setup when device is in AP mode.
// Uses CSS-only Blink/LNbits toggle (no JavaScript).
static const char SETUP_PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="sk">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FIAT HELL Setup</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;max-width:480px;margin:0 auto;padding:16px;background:#111;color:#eee}
h1{color:#f90;font-size:1.3em;margin:0 0 2px}
.sub{color:#555;font-size:.85em;margin:0 0 20px}
h2{color:#888;font-size:.8em;margin:16px 0 8px;text-transform:uppercase;letter-spacing:.08em}
label{display:block;color:#999;font-size:.85em;margin-top:10px}
input[type=text],input[type=password],input[type=number]{
  display:block;width:100%;padding:10px;margin-top:4px;
  background:#1e1e1e;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em}
input[name=funding]{position:absolute;opacity:0;width:0;height:0}
.ftabs{display:flex;gap:8px;margin:10px 0 14px}
.ftab{flex:1;text-align:center;padding:10px;border:2px solid #444;border-radius:8px;cursor:pointer;font-size:1em;color:#999}
#fund_blink:checked~.ftabs label[for=fund_blink],
#fund_lnbits:checked~.ftabs label[for=fund_lnbits]{border-color:#f90;color:#f90;background:#1a0d00}
#fund_blink:not(:checked)~.blink-fields{display:none}
#fund_lnbits:not(:checked)~.lnbits-fields{display:none}
.card{background:#181818;border-radius:10px;padding:14px;margin-top:12px}
.hint{font-size:.78em;color:#555;margin:4px 0 0}
button{display:block;width:100%;margin-top:24px;padding:14px;
       background:#f90;color:#000;border:none;border-radius:8px;
       font-size:1.1em;font-weight:bold;cursor:pointer}
button:active{background:#c70}
</style>
</head>
<body>
<h1>&#9889; FIAT HELL</h1>
<p class="sub">Nastavenia zariadenia</p>
<form method="POST" action="/setup/save">

<div class="card">
<h2>WiFi</h2>
<p class="hint">Vyplň iba ak chceš zmeniť sieť. Prázdne pole zachová aktuálne nastavenie.</p>
<label>SSID (názov siete)<input type="text" name="wifi_ssid" value="%%WIFI_SSID%%"></label>
<label>Heslo WiFi<input type="password" name="wifi_password" autocomplete="new-password"></label>
</div>

<div class="card">
<h2>Financovanie</h2>
<input type="radio" name="funding" id="fund_blink" value="Blink" %%CHECKED_BLINK%%>
<input type="radio" name="funding" id="fund_lnbits" value="LNbits" %%CHECKED_LNBITS%%>
<div class="ftabs">
  <label class="ftab" for="fund_blink">Blink</label>
  <label class="ftab" for="fund_lnbits">LNbits</label>
</div>
<div class="blink-fields">
  <label>Blink API kľúč<input type="text" name="blink_apikey" value="%%BLINK_APIKEY%%"></label>
  <label>Blink Wallet ID<input type="text" name="blink_wallet" value="%%BLINK_WALLET%%"></label>
</div>
<div class="lnbits-fields">
  <label>Admin kľúč<input type="text" name="adminkey" value="%%ADMINKEY%%"></label>
  <label>Read kľúč<input type="text" name="readkey" value="%%READKEY%%"></label>
  <label>LNURL base URL<input type="text" name="lnurl_base" value="%%LNURL_BASE%%"></label>
  <label>LNURL secret<input type="text" name="lnurl_secret" value="%%LNURL_SECRET%%"></label>
</div>
</div>

<div class="card">
<h2>Mena 1</h2>
<label>Kód meny (napr. EUR)<input type="text" name="cur1_code" value="%%CUR1_CODE%%" maxlength="8"></label>
<label>Sumy bankoviek, CSV (napr. 5,10,20,50)<input type="text" name="cur1_bills" value="%%CUR1_BILLS%%"></label>
<p class="hint">Nominálne hodnoty bankoviek, ktoré prijíma NV10 — oddelené čiarkou.</p>
<label>Max suma (0 = bez limitu)<input type="number" name="cur1_max" value="%%CUR1_MAX%%" min="0" step="1"></label>
<label>Poplatok %<input type="number" name="cur1_charge" value="%%CUR1_CHARGE%%" step="0.01" min="0"></label>
</div>

<div class="card">
<h2>ATM</h2>
<label>Názov<input type="text" name="atm_title" value="%%ATM_TITLE%%"></label>
<label>Podnadpis<input type="text" name="atm_subtitle" value="%%ATM_SUBTITLE%%"></label>
<label>Popis<input type="text" name="atm_desc" value="%%ATM_DESC%%"></label>
<label>Heslo pre AP a portál (meno: admin)<input type="password" name="ap_password" autocomplete="new-password" placeholder="ponechať nezmenené"></label>
<p class="hint">Heslo pre WiFi sieť "LN ATM-xxx" a pre webový konfig portál. Prázdne = zachovať aktuálne.</p>
</div>

<div class="card">
<h2>Aktualizácia firmvéru</h2>
<p>Aktuálna verzia: <strong>%%FW_VERSION%%</strong></p>
<p class="hint">Zariadenie musí byť pripojené na internet (WiFi).</p>
<form method="POST" action="/setup/ota">
  <label>Vyberte verziu<select name="ota_filename" style="margin-top:6px">%%OTA_OPTIONS%%</select></label>
  <button type="submit" style="margin-top:10px;background:#444;color:#eee;font-size:.95em;padding:10px">&#8593; Nainštalovať</button>
</form>
</div>

<button type="submit">Uloziť a reštartovať</button>
</form>
</body>
</html>)rawliteral";
