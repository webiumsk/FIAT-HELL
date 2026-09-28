#pragma once

// Mobile-friendly config portal page served at /setup when device is in AP mode.
// Uses CSS-only Blink/LNbits toggle (no JavaScript).
// OTA upload is a separate form OUTSIDE the main settings form.
static const char SETUP_PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
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
input[type=text],input[type=password],input[type=number],input[type=file]{
  display:block;width:100%;padding:10px;margin-top:4px;
  background:#1e1e1e;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em}
input[name=funding]{position:absolute;opacity:0;width:0;height:0}
.ftabs{display:flex;gap:8px;margin:10px 0 14px}
.ftab{flex:1;text-align:center;padding:10px;border:2px solid #444;border-radius:8px;cursor:pointer;font-size:1em;color:#999}
#fund_blink:checked~.ftabs label[for=fund_blink],
#fund_flash:checked~.ftabs label[for=fund_flash],
#fund_lnbits:checked~.ftabs label[for=fund_lnbits]{border-color:#f90;color:#f90;background:#1a0d00}
#fund_lnbits:checked~.blink-fields{display:none}
#fund_lnbits:not(:checked)~.lnbits-fields{display:none}
.card{background:#181818;border-radius:10px;padding:14px;margin-top:12px}
.hint{font-size:.78em;color:#555;margin:4px 0 0}
.optional{color:#555;font-size:.75em;text-transform:none;letter-spacing:0;font-weight:normal}
button{display:block;width:100%;margin-top:24px;padding:14px;
       background:#f90;color:#000;border:none;border-radius:8px;
       font-size:1.1em;font-weight:bold;cursor:pointer}
button:active{background:#c70}
button.secondary{background:#444;color:#eee;font-size:.95em;padding:10px;font-weight:normal;margin-top:10px}
#wifi-list{margin-top:8px}
.wifi-item{display:flex;justify-content:space-between;align-items:center;width:100%;
  margin:6px 0 0;padding:10px 12px;background:#1e1e1e;color:#eee;
  border:1px solid #444;border-radius:6px;font-size:.95em;cursor:pointer;text-align:left}
.wifi-item:active,.wifi-item.sel{border-color:#f90;background:#1a0d00}
.wifi-item .wb{color:#888;font-size:.8em;margin-left:8px}
.wifi-item .lk{color:#f90;margin-right:6px}
</style>
</head>
<body>
<h1>&#9889; FIAT HELL</h1>
<p class="sub">Device settings</p>

<form method="POST" action="/setup/save">

<div class="card">
<h2>WiFi</h2>
<p class="hint">Fill in only if you want to change networks. Empty fields keep the current setting.</p>
<button type="button" class="secondary" onclick="scanWifi(this)">&#128269; Scan for networks</button>
<div id="wifi-list"></div>
<label>SSID (network name)<input type="text" id="wifi_ssid" name="wifi_ssid" value="%%WIFI_SSID%%" autocomplete="off"></label>
<label>WiFi password<input type="password" id="wifi_password" name="wifi_password" autocomplete="new-password"></label>
</div>

<div class="card">
<h2>BTC price source</h2>
<select name="ratesource" style="display:block;width:100%;padding:10px;margin-top:4px;background:#1e1e1e;color:#eee;border:1px solid #444;border-radius:6px;font-size:1em">
  <option value="CoinYEP"     %%RS_COINYEP%%>CoinYEP</option>
  <option value="Kraken"      %%RS_KRAKEN%%>Kraken</option>
  <option value="ExchangeApi" %%RS_EXCHANGEAPI%%>ExchangeApi (Fawaz)</option>
</select>
<p class="hint">If one source stops working (HTTP -1 / connection refused in the log), try another.</p>
</div>

<div class="card">
<h2>Funding</h2>
<input type="radio" name="funding" id="fund_blink" value="Blink" %%CHECKED_BLINK%%>
<input type="radio" name="funding" id="fund_flash" value="Flash" %%CHECKED_FLASH%%>
<input type="radio" name="funding" id="fund_lnbits" value="LNbits" %%CHECKED_LNBITS%%>
<div class="ftabs">
  <label class="ftab" for="fund_blink">Blink</label>
  <label class="ftab" for="fund_flash">Flash</label>
  <label class="ftab" for="fund_lnbits">LNbits</label>
</div>
<div class="blink-fields">
  <label>API key (Blink / Flash)<input type="text" name="blink_apikey" value="%%BLINK_APIKEY%%"></label>
  <div class="hint"><a href="/flashkey" style="color:#f90">⚡ No Flash API key? Get one here</a></div>
  <label>Wallet ID (Blink / Flash)<input type="text" name="blink_wallet" value="%%BLINK_WALLET%%"></label>
</div>
<div class="lnbits-fields">
  <label>Admin key<input type="text" name="adminkey" value="%%ADMINKEY%%"></label>
  <label>Read key<input type="text" name="readkey" value="%%READKEY%%"></label>
  <label>LNURL base URL<input type="text" name="lnurl_base" value="%%LNURL_BASE%%"></label>
  <label>LNURL secret<input type="text" name="lnurl_secret" value="%%LNURL_SECRET%%"></label>
</div>
</div>

<div class="card">
<h2>Currency 1</h2>
<label>Currency code (e.g. EUR)<input type="text" name="cur1_code" value="%%CUR1_CODE%%" maxlength="8"></label>
<label>Bill amounts, CSV (e.g. 5,10,20,50)<input type="text" name="cur1_bills" value="%%CUR1_BILLS%%"></label>
<p class="hint">Denominations accepted by the NV10, comma-separated.</p>
<label>Max amount (0 = no limit)<input type="number" name="cur1_max" value="%%CUR1_MAX%%" min="0" step="1"></label>
<label>Fee %<input type="number" name="cur1_charge" value="%%CUR1_CHARGE%%" step="0.01" min="0"></label>
</div>

<div class="card">
<h2>Currency 2 <span class="optional">(optional, LNbits)</span></h2>
<p class="hint">Leave empty if you don't use a second currency.</p>
<label>Currency code<input type="text" name="cur2_code" value="%%CUR2_CODE%%" maxlength="8"></label>
<label>LNURL base URL<input type="text" name="cur2_lnurl_base" value="%%CUR2_LNURL_BASE%%"></label>
<label>LNURL secret<input type="text" name="cur2_lnurl_secret" value="%%CUR2_LNURL_SECRET%%"></label>
<label>Bill amounts, CSV<input type="text" name="cur2_bills" value="%%CUR2_BILLS%%"></label>
<label>Max amount (0 = no limit)<input type="number" name="cur2_max" value="%%CUR2_MAX%%" min="0" step="1"></label>
<label>Fee %<input type="number" name="cur2_charge" value="%%CUR2_CHARGE%%" step="0.01" min="0"></label>
</div>

<div class="card">
<h2>Currency 3 <span class="optional">(optional, LNbits)</span></h2>
<p class="hint">Leave empty if you don't use a third currency.</p>
<label>Currency code<input type="text" name="cur3_code" value="%%CUR3_CODE%%" maxlength="8"></label>
<label>LNURL base URL<input type="text" name="cur3_lnurl_base" value="%%CUR3_LNURL_BASE%%"></label>
<label>LNURL secret<input type="text" name="cur3_lnurl_secret" value="%%CUR3_LNURL_SECRET%%"></label>
<label>Bill amounts, CSV<input type="text" name="cur3_bills" value="%%CUR3_BILLS%%"></label>
<label>Max amount (0 = no limit)<input type="number" name="cur3_max" value="%%CUR3_MAX%%" min="0" step="1"></label>
<label>Fee %<input type="number" name="cur3_charge" value="%%CUR3_CHARGE%%" step="0.01" min="0"></label>
</div>

<div class="card">
<h2>ATM</h2>
<label>Title<input type="text" name="atm_title" value="%%ATM_TITLE%%"></label>
<label>Subtitle<input type="text" name="atm_subtitle" value="%%ATM_SUBTITLE%%"></label>
<label>Description<input type="text" name="atm_desc" value="%%ATM_DESC%%"></label>
<label>Password for AP and portal (user: admin)<input type="password" name="ap_password" autocomplete="new-password" placeholder="leave unchanged"></label>
<p class="hint">Password for the "LN ATM-xxx" WiFi network and the web config portal. Empty = keep current.</p>
</div>

<button type="submit">Save and restart</button>
</form>

<div class="card">
<h2>Firmware update</h2>
<p>Current version: <strong>%%FW_VERSION%%</strong></p>
<p class="hint">Upload a .bin file from your phone — the device does not need internet access.</p>
<form method="POST" action="/setup/ota" enctype="multipart/form-data">
  <label>Choose .bin file<input type="file" name="firmware" accept=".bin,application/octet-stream" required></label>
  <button type="submit" class="secondary">&#8593; Upload firmware</button>
</form>
</div>

<script>
function scanWifi(btn){
  var lst=document.getElementById('wifi-list');
  var orig=btn.textContent;
  btn.disabled=true;btn.textContent='Scanning…';
  lst.innerHTML='<p class="hint">Scanning the 2.4GHz band…</p>';
  var tries=0;
  function poll(){
    tries++;
    fetch('/setup/wifi-scan',{cache:'no-store'}).then(function(r){
      if(!r.ok) throw new Error('HTTP '+r.status);
      return r.json();
    }).then(function(res){
      // Async scan: {"scanning":true} means keep polling; an array = results.
      if(res&&res.scanning){
        if(tries>12){throw new Error('scan timed out');}
        setTimeout(poll,1500);return;
      }
      render(res);
    }).catch(function(e){
      // The scan briefly disrupts the AP; a dropped poll is expected - retry.
      if(tries<=12){setTimeout(poll,1500);return;}
      btn.disabled=false;btn.textContent=orig;
      lst.innerHTML='<p class="hint">Scan failed: '+e.message+'. Enter the SSID manually below.</p>';
    });
  }
  function render(arr){
    btn.disabled=false;btn.textContent=orig;
    if(!arr||!arr.length){lst.innerHTML='<p class="hint">No networks found. Enter the SSID manually below.</p>';return;}
    var h='';
    arr.forEach(function(n){
      var s=(n.ssid||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
      var bars=n.rssi>-55?'▂▄▆█':(n.rssi>-70?'▂▄▆_':(n.rssi>-80?'▂▄__':'▂___'));
      var lk=n.secure?'<span class="lk">&#128274;</span>':'';
      h+='<button type="button" class="wifi-item" data-ssid="'+s+'"><span>'+lk+s+'</span><span class="wb">'+bars+' '+n.rssi+' dBm</span></button>';
    });
    lst.innerHTML=h;
    Array.prototype.forEach.call(lst.querySelectorAll('.wifi-item'),function(b){
      b.addEventListener('click',function(){
        Array.prototype.forEach.call(lst.querySelectorAll('.wifi-item'),function(x){x.classList.remove('sel');});
        b.classList.add('sel');
        document.getElementById('wifi_ssid').value=b.getAttribute('data-ssid');
        var p=document.getElementById('wifi_password');p.value='';p.focus();
      });
    });
  }
  poll();
}
</script>
</body>
</html>)rawliteral";
