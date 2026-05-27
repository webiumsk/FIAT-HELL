import JSZip from 'jszip';
import { checkBrowserSupport, connectAndFlash } from './flash.js';
import { FW_VERSION } from './config.js';

/* ══════════════════════════════════════════════════════════════
   Panel & accordion helpers
══════════════════════════════════════════════════════════════ */
function showPanel(name) {
  document.querySelectorAll('.page-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.step').forEach(s => s.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  document.getElementById('tab-' + name).classList.add('active');
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

function toggleSection(id) {
  const body = document.getElementById('body-' + id);
  const chev = document.getElementById('chev-' + id);
  const isOpen = body.classList.contains('open');
  body.classList.toggle('open', !isOpen);
  chev.classList.toggle('open', !isOpen);
}

function onFundingChange() {
  const isLNbits = document.getElementById('fund_lnbits').checked;
  document.querySelectorAll('.lnbits-fields').forEach(el => el.style.display = isLNbits ? '' : 'none');
  document.querySelectorAll('.blink-fields').forEach(el => el.style.display  = isLNbits ? 'none' : '');
}

/* ══════════════════════════════════════════════════════════════
   Form helpers
══════════════════════════════════════════════════════════════ */
function v(id)   { return document.getElementById(id).value.trim(); }
function num(id) { const val = v(id); return val === '' ? '' : val; }

/* ══════════════════════════════════════════════════════════════
   JSON generators (order MUST match ConfigService load order)
══════════════════════════════════════════════════════════════ */
function makeElementsJson() {
  return [
    { name: 'password',    value: v('ap_password') || 'changeme' },
    { name: 'atmdesc',     value: v('atm_desc') },
    { name: 'atmsubtitle', value: v('atm_subtitle') },
    { name: 'atmtitle',    value: v('atm_title') || 'FIAT HELL' },
  ];
}

function makeGuiJson() {
  const funding  = document.querySelector('input[name="funding"]:checked').value;
  const rate     = document.querySelector('input[name="ratesource"]:checked').value;
  const animated = document.querySelector('input[name="animated"]:checked').value;
  const rateIndex = { CoinGecko: 1, ExchangeApi: 2, CoinYEP: 3, Kraken: 4 }[rate] ?? 1;
  return [
    { name: 'fundingsource', value: ['Blink', 'LNbits'], checked: funding === 'Blink' ? 1 : 2 },
    { name: 'ratesource',    value: ['CoinGecko', 'ExchangeApi', 'CoinYEP', 'Kraken'], checked: rateIndex },
    { name: 'animated',      value: ['No', 'Yes'], checked: animated === 'Yes' ? 2 : 1 },
  ];
}

function makeFirstJson() {
  const isLNbits = document.getElementById('fund_lnbits').checked;
  const lnurlValue = [v('cur1_lnurl_base'), v('cur1_lnurl_secret'), v('cur1_lnurl_atm') || v('cur1_code')].join(',');
  return [
    { name: 'blinkapikey',   value: isLNbits ? '' : v('cur1_blink_apikey') },
    { name: 'blinkwalletid', value: isLNbits ? '' : v('cur1_blink_wallet') },
    { name: 'lnurl',         value: isLNbits ? lnurlValue : '' },
    { name: 'adminkey',      value: isLNbits ? v('cur1_adminkey') : '' },
    { name: 'readkey',       value: isLNbits ? v('cur1_readkey')  : '' },
    { name: 'currencyOne',   value: v('cur1_code') },
    { name: 'billmech',      value: v('cur1_bills') },
    { name: 'maxamount',     value: num('cur1_max') },
    { name: 'charge1',       value: num('cur1_charge') },
  ];
}

function makeSecondJson() {
  if (!v('cur2_code')) return null;
  const lnurlValue = [v('cur2_lnurl_base'), v('cur2_lnurl_secret'), v('cur2_lnurl_atm') || v('cur2_code')].join(',');
  return [
    { name: 'currencyTwo', value: v('cur2_code') },
    { name: 'lnurl2',      value: lnurlValue },
    { name: 'billmech2',   value: v('cur2_bills') },
    { name: 'maxamount2',  value: num('cur2_max') },
    { name: 'charge2',     value: num('cur2_charge') },
  ];
}

function makeThirdJson() {
  if (!v('cur3_code')) return null;
  const lnurlValue = [v('cur3_lnurl_base'), v('cur3_lnurl_secret'), v('cur3_lnurl_atm') || v('cur3_code')].join(',');
  return [
    { name: 'currencyThree', value: v('cur3_code') },
    { name: 'lnurl3',        value: lnurlValue },
    { name: 'billmech3',     value: v('cur3_bills') },
    { name: 'maxamount3',    value: num('cur3_max') },
    { name: 'charge3',       value: num('cur3_charge') },
  ];
}

function makeConfigFiles() {
  const files = {
    '/elements.json': makeElementsJson(),
    '/gui.json':      makeGuiJson(),
    '/first.json':    makeFirstJson(),
  };
  const second = makeSecondJson();
  if (second) files['/second.json'] = second;
  const third = makeThirdJson();
  if (third) files['/third.json'] = third;
  return files;
}

/* ══════════════════════════════════════════════════════════════
   Download config ZIP
══════════════════════════════════════════════════════════════ */
async function downloadConfigZip() {
  if (!v('cur1_code')) {
    alert('Vyplň aspoň Kód meny pre Mena 1 (napr. EUR)');
    if (!document.getElementById('body-cur1').classList.contains('open')) toggleSection('cur1');
    return;
  }

  const zip = new JSZip();
  zip.file('elements.json', JSON.stringify(makeElementsJson(), null, 2));
  zip.file('gui.json',      JSON.stringify(makeGuiJson(),      null, 2));
  zip.file('first.json',    JSON.stringify(makeFirstJson(),    null, 2));
  const second = makeSecondJson();
  if (second) zip.file('second.json', JSON.stringify(second, null, 2));
  const third = makeThirdJson();
  if (third)  zip.file('third.json',  JSON.stringify(third,  null, 2));

  const blob = await zip.generateAsync({ type: 'blob' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href     = url;
  a.download = 'fiat-hell-config.zip';
  a.click();
  URL.revokeObjectURL(url);

  const toast = document.getElementById('toast');
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 2800);
}

/* ══════════════════════════════════════════════════════════════
   Clear form
══════════════════════════════════════════════════════════════ */
function clearForm() {
  if (!confirm('Naozaj vymazať všetky zadané hodnoty?')) return;
  document.querySelectorAll('input[type="text"], input[type="password"], input[type="number"]')
    .forEach(el => { el.value = el.defaultValue || ''; });
  document.querySelectorAll('input[type="radio"]').forEach(r => { r.checked = r.defaultChecked; });
  onFundingChange();
  saveToStorage();
}

/* ══════════════════════════════════════════════════════════════
   LocalStorage persistence
══════════════════════════════════════════════════════════════ */
const STORAGE_KEY = 'fiat-hell-flasher-v1';

function saveToStorage() {
  const data = {};
  document.querySelectorAll('input[type="text"], input[type="password"], input[type="number"]')
    .forEach(el => { if (el.id) data[el.id] = el.value; });
  document.querySelectorAll('input[type="radio"]:checked')
    .forEach(r => { data['radio_' + r.name] = r.value; });
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
}

function loadFromStorage() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const data = JSON.parse(raw);
    Object.entries(data).forEach(([key, val]) => {
      if (key.startsWith('radio_')) {
        const radio = document.querySelector(`input[name="${key.slice(6)}"][value="${val}"]`);
        if (radio) radio.checked = true;
      } else {
        const el = document.getElementById(key);
        if (el) el.value = val;
      }
    });
    onFundingChange();
  } catch (_) {}
}

/* ══════════════════════════════════════════════════════════════
   Flash UI helpers
══════════════════════════════════════════════════════════════ */
function getTerminal()  { return document.getElementById('terminal'); }
function getProgress()  { return document.getElementById('progress-fill'); }
function getProgWrap()  { return document.getElementById('progress-wrap'); }
function getProgText()  { return document.getElementById('progress-text'); }

function clearTerminal() {
  const t = getTerminal();
  t.textContent = '';
  t.classList.add('visible');
  getProgWrap().classList.add('visible');
  getProgText().style.display = '';
  setProgress(0);
}

function appendToTerminal(msg) {
  const t = getTerminal();
  t.textContent += msg + '\n';
  t.scrollTop = t.scrollHeight;
}

function setProgress(pct) {
  getProgress().style.width = pct + '%';
  getProgText().textContent = pct < 100 ? `${pct}%` : 'Dokončené';
}

function setFlashBusy(busy) {
  document.getElementById('btn-flash-config').disabled = busy;
  document.getElementById('btn-flash-only').disabled   = busy;
}

/* ══════════════════════════════════════════════════════════════
   Flash actions
══════════════════════════════════════════════════════════════ */
async function connectAndFlashWithConfig() {
  if (!v('cur1_code')) {
    alert('Pre nahranie konfigurácie vyplň aspoň Kód meny 1 (napr. EUR).');
    showPanel('config');
    if (!document.getElementById('body-cur1').classList.contains('open')) toggleSection('cur1');
    return;
  }
  clearTerminal();
  setFlashBusy(true);
  try {
    await connectAndFlash({ log: appendToTerminal, setProgress, configFiles: makeConfigFiles() });
  } catch (e) {
    appendToTerminal('');
    appendToTerminal('✗ Chyba: ' + e.message);
    console.error(e);
  } finally {
    setFlashBusy(false);
  }
}

async function connectAndFlashOnly() {
  clearTerminal();
  setFlashBusy(true);
  try {
    await connectAndFlash({ log: appendToTerminal, setProgress, configFiles: null });
  } catch (e) {
    appendToTerminal('');
    appendToTerminal('✗ Chyba: ' + e.message);
    console.error(e);
  } finally {
    setFlashBusy(false);
  }
}

/* ══════════════════════════════════════════════════════════════
   Init
══════════════════════════════════════════════════════════════ */
document.addEventListener('DOMContentLoaded', () => {
  // Show firmware version
  const verEl = document.getElementById('fw-version');
  if (verEl && FW_VERSION !== 'FW_VERSION_PLACEHOLDER') verEl.textContent = 'v' + FW_VERSION;

  // Check Web Serial support
  const support = checkBrowserSupport();
  if (!support.ok) {
    const errEl = document.getElementById('browser-error');
    errEl.textContent = support.reason;
    errEl.style.display = '';
    document.getElementById('flash-actions').style.display = 'none';
  }

  loadFromStorage();
  onFundingChange();

  document.addEventListener('input',  saveToStorage);
  document.addEventListener('change', saveToStorage);

  toggleSection('general');
  toggleSection('cur1');

  // Expose functions for inline onclick handlers in HTML
  Object.assign(window, {
    showPanel,
    toggleSection,
    onFundingChange,
    downloadConfigZip,
    clearForm,
    connectAndFlashWithConfig,
    connectAndFlashOnly,
  });
});
