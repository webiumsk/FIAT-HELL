import JSZip from 'jszip';
import { checkBrowserSupport, connectAndFlash, uploadConfigOnly } from './flash.js';
import { FW_VERSION, BOARDS, DEFAULT_BOARD, GITHUB_REPO } from './config.js';

/* ══════════════════════════════════════════════════════════════
   Panel & accordion helpers
══════════════════════════════════════════════════════════════ */
function showPanel(name) {
  document.querySelectorAll('.page-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.step').forEach(s => {
    s.classList.remove('active');
    s.setAttribute('aria-selected', 'false');
  });
  document.getElementById('panel-' + name).classList.add('active');
  const tab = document.getElementById('tab-' + name);
  tab.classList.add('active');
  tab.setAttribute('aria-selected', 'true');
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

function toggleSection(id) {
  const body = document.getElementById('body-' + id);
  const chev = document.getElementById('chev-' + id);
  const isOpen = body.classList.contains('open');
  body.classList.toggle('open', !isOpen);
  chev.classList.toggle('open', !isOpen);
  const btn = document.querySelector(`[aria-controls="body-${id}"]`);
  if (btn) btn.setAttribute('aria-expanded', String(!isOpen));
}

function onFundingChange() {
  // Blink and Flash share the same credential fields (Galoy API).
  const isLNbits = document.getElementById('fund_lnbits').checked;
  document.querySelectorAll('.lnbits-fields').forEach(el => el.style.display = isLNbits ? '' : 'none');
  document.querySelectorAll('.blink-fields').forEach(el => el.style.display  = isLNbits ? 'none' : '');
}

/* ══════════════════════════════════════════════════════════════
   Board selection (S3 vs WT32 — different binaries and offsets)
══════════════════════════════════════════════════════════════ */
function selectedBoard() {
  const checked = document.querySelector('input[name="board"]:checked');
  return (checked && BOARDS[checked.value]) ? checked.value : DEFAULT_BOARD;
}

function onBoardChange() {
  const board = BOARDS[selectedBoard()];
  const bootHint = document.getElementById('boot-hint');
  if (bootHint) {
    bootHint.textContent = board.manualBoot
      ? 'Manuálny boot: drž BOOT, stlač RESET, pusť a klikni Flash.'
      : 'Doska sa resetne automaticky; ak flash nenabehne, drž BOOT pri pripájaní.';
  }
  populateVersionDropdown().catch(e => console.error('Releases load failed:', e));
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
    { name: 'password',    value: v('ap_password') },
    { name: 'atmdesc',     value: v('atm_desc') },
    { name: 'atmsubtitle', value: v('atm_subtitle') },
    { name: 'atmtitle',    value: v('atm_title') || 'FIAT HELL' },
  ];
}

function validateConfig() {
  const pwd = v('ap_password');
  if (!pwd || pwd.length < 8 || pwd === 'changeme') {
    alert('Heslo pre AP portál musí mať aspoň 8 znakov a nesmie byť "changeme".');
    showPanel('config');
    if (!document.getElementById('body-general').classList.contains('open')) toggleSection('general');
    return false;
  }
  if (!v('cur1_code')) {
    alert('Vyplň aspoň Kód meny pre Mena 1 (napr. EUR)');
    showPanel('config');
    if (!document.getElementById('body-cur1').classList.contains('open')) toggleSection('cur1');
    return false;
  }
  return true;
}

function makeGuiJson() {
  const funding  = document.querySelector('input[name="funding"]:checked').value;
  const rate     = document.querySelector('input[name="ratesource"]:checked').value;
  const animated = document.querySelector('input[name="animated"]:checked').value;
  // Firmware's ConfigService still parses the legacy 4-entry array; keep that
  // shape so older firmwares stay compatible. UI only exposes 3 options.
  const rateIndex = { CoinGecko: 1, ExchangeApi: 2, CoinYEP: 3, Kraken: 4 }[rate] ?? 3;
  // Funding value array order must match pagegui.h: Blink, LNbits, Flash.
  const fundingIndex = { Blink: 1, LNbits: 2, Flash: 3 }[funding] ?? 1;
  return [
    { name: 'fundingsource', value: ['Blink', 'LNbits', 'Flash'], checked: fundingIndex },
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

function makeWifiJson() {
  const ssid = v('wifi_ssid');
  if (!ssid) return null;
  return { ssid, password: document.getElementById('wifi_password').value };
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
  const wifi = makeWifiJson();
  if (wifi) files['/wifi.json'] = wifi;
  return files;
}

/* ══════════════════════════════════════════════════════════════
   Download config ZIP
══════════════════════════════════════════════════════════════ */
async function downloadConfigZip() {
  if (!validateConfig()) return;

  const zip = new JSZip();
  zip.file('elements.json', JSON.stringify(makeElementsJson(), null, 2));
  zip.file('gui.json',      JSON.stringify(makeGuiJson(),      null, 2));
  zip.file('first.json',    JSON.stringify(makeFirstJson(),    null, 2));
  const second = makeSecondJson();
  if (second) zip.file('second.json', JSON.stringify(second, null, 2));
  const third = makeThirdJson();
  if (third)  zip.file('third.json',  JSON.stringify(third,  null, 2));
  const wifi = makeWifiJson();
  if (wifi)   zip.file('wifi.json',   JSON.stringify(wifi,   null, 2));

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

// Fields explicitly persisted to localStorage even though they're password-type.
// These secrets are masked in the UI only to deter shoulder-surfing — the
// operator still wants them remembered between sessions on their own machine.
const PERSISTED_SECRETS = new Set(['cur1_blink_apikey']);

function saveToStorage() {
  const data = {};
  // password-type inputs are intentionally excluded — except those in
  // PERSISTED_SECRETS (e.g. the Blink API key shown as a masked field)
  document.querySelectorAll('input[type="text"], input[type="number"]')
    .forEach(el => { if (el.id) data[el.id] = el.value; });
  PERSISTED_SECRETS.forEach(id => {
    const el = document.getElementById(id);
    if (el) data[id] = el.value;
  });
  document.querySelectorAll('input[type="radio"]:checked')
    .forEach(r => { data['radio_' + r.name] = r.value; });
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
}

/* ══════════════════════════════════════════════════════════════
   Secret input masking (Blink API key etc.)
══════════════════════════════════════════════════════════════ */
function fingerprintSecret(s) {
  if (!s) return '';
  if (s.length <= 8) return '••• (uložené, ' + s.length + ' znakov)';
  return s.slice(0, 4) + ' ••• ' + s.slice(-4);
}

function updateSecretFingerprints() {
  document.querySelectorAll('[id^="fp_"]').forEach(fpEl => {
    const inputId = fpEl.id.slice(3);
    const input = document.getElementById(inputId);
    if (input) fpEl.textContent = input.value ? 'Uložené: ' + fingerprintSecret(input.value) : '';
  });
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
   GitHub releases — fetch, cache, dropdown
══════════════════════════════════════════════════════════════ */
const RELEASES_CACHE_KEY = 'gh-releases-v2';
const RELEASES_CACHE_TTL_MS = 5 * 60 * 1000; // 5 minutes

// Cache of release id → flashParts mapping for the currently selected board.
const releaseFlashParts = new Map();

/**
 * Resolve a release's assets into flash parts for one board.
 * New releases carry board-suffixed assets (firmware-s3.bin, firmware-wt32.bin,
 * per-board bootloader/partitions) plus a shared boot_app0.bin. Releases from
 * before the dual-board split carry unsuffixed assets and are S3-only.
 * Returns { parts, legacy } or null when the release can't flash this board.
 */
function resolveBoardParts(assetsByName, boardKey) {
  const board = BOARDS[boardKey];
  const get = n => assetsByName.get(n.toLowerCase());

  const trySuffix = suffix => {
    const parts = [];
    for (const name of ['bootloader.bin', 'partitions.bin', 'firmware.bin']) {
      const a = get(name.replace('.bin', suffix + '.bin'));
      if (!a) return null;
      parts.push({ path: a.browser_download_url, offset: board.offsets[name] });
    }
    const boot = get('boot_app0.bin') || get('boot_app0' + suffix + '.bin');
    if (!boot) return null;
    parts.push({ path: boot.browser_download_url, offset: board.offsets['boot_app0.bin'] });
    return parts;
  };

  let parts = trySuffix(board.assetSuffix);
  if (parts) return { parts, legacy: false };
  if (boardKey === 's3') {
    parts = trySuffix('');
    if (parts) return { parts, legacy: true };
  }
  return null;
}

async function fetchReleases() {
  // Try sessionStorage cache first (raw asset lists, board-independent)
  try {
    const cached = sessionStorage.getItem(RELEASES_CACHE_KEY);
    if (cached) {
      const { at, data } = JSON.parse(cached);
      if (Date.now() - at < RELEASES_CACHE_TTL_MS) return data;
    }
  } catch (_) {}

  const url = `https://api.github.com/repos/${GITHUB_REPO}/releases?per_page=30`;
  const resp = await fetch(url, { headers: { Accept: 'application/vnd.github+json' } });
  if (!resp.ok) throw new Error(`GitHub API HTTP ${resp.status}`);
  const all = await resp.json();

  const releases = all
    .filter(r => !r.draft)
    .map(r => ({
      id: r.id,
      tag: r.tag_name,
      name: r.name || r.tag_name,
      date: r.published_at ? r.published_at.slice(0, 10) : '',
      prerelease: !!r.prerelease,
      assets: (r.assets || []).map(a => ({
        name: a.name.toLowerCase(),
        url: a.browser_download_url,
      })),
    }));

  try {
    sessionStorage.setItem(RELEASES_CACHE_KEY, JSON.stringify({ at: Date.now(), data: releases }));
  } catch (_) {}
  return releases;
}

async function populateVersionDropdown() {
  const sel = document.getElementById('fw-version-select');
  if (!sel) return;
  releaseFlashParts.clear();
  const boardKey = selectedBoard();

  let releases = [];
  let error = null;
  try {
    releases = await fetchReleases();
  } catch (e) {
    error = e.message;
  }

  // Clear all options except the "local" first option
  while (sel.options.length > 1) sel.remove(1);
  sel.value = '__local__';

  if (error) {
    const opt = document.createElement('option');
    opt.disabled = true;
    opt.textContent = `Chyba načítania GitHub releases (${error})`;
    sel.add(opt);
    return;
  }

  const flashable = [];
  for (const r of releases) {
    const assetsByName = new Map(r.assets.map(a => [a.name, { browser_download_url: a.url }]));
    const resolved = resolveBoardParts(assetsByName, boardKey);
    if (resolved) flashable.push({ ...r, ...resolved });
  }

  if (flashable.length === 0) {
    const opt = document.createElement('option');
    opt.disabled = true;
    opt.textContent = `Žiadne release-y pre ${BOARDS[boardKey].label}`;
    sel.add(opt);
    return;
  }

  for (const r of flashable) {
    const key = 'release:' + r.id;
    releaseFlashParts.set(key, r.parts);
    const opt = document.createElement('option');
    opt.value = key;
    const tag = r.tag.startsWith('v') ? r.tag : 'v' + r.tag;
    const pre = r.prerelease ? ' [pre-release]' : '';
    const legacy = r.legacy ? ' [legacy S3]' : '';
    opt.textContent = `${tag}${pre}${legacy}  —  ${r.date}`;
    sel.add(opt);
  }

  // Default selection: newest non-prerelease, else newest overall.
  const defaultRelease = flashable.find(r => !r.prerelease) || flashable[0];
  if (defaultRelease) sel.value = 'release:' + defaultRelease.id;
  onVersionChange();
}

function selectedFlashParts() {
  const localParts = BOARDS[selectedBoard()].localParts;
  const sel = document.getElementById('fw-version-select');
  if (!sel || sel.value === '__local__') return localParts;
  return releaseFlashParts.get(sel.value) || localParts;
}

function onVersionChange() {
  const sel = document.getElementById('fw-version-select');
  const hint = document.getElementById('fw-version-hint');
  if (!sel || !hint) return;
  if (sel.value === '__local__') {
    hint.textContent = 'Verzia zabundlená do tejto stránky pri jej deploye.';
  } else {
    const opt = sel.options[sel.selectedIndex];
    hint.textContent = `.bin súbory sa stiahnu z GitHub release: ${opt.textContent.trim()}`;
  }
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
  const cfgOnly = document.getElementById('btn-config-only');
  if (cfgOnly) cfgOnly.disabled = busy;
}

/* ══════════════════════════════════════════════════════════════
   Flash actions
══════════════════════════════════════════════════════════════ */
async function connectAndFlashWithConfig() {
  if (!validateConfig()) return;
  clearTerminal();
  setFlashBusy(true);
  try {
    await connectAndFlash({
      log: appendToTerminal,
      setProgress,
      configFiles: makeConfigFiles(),
      flashParts: selectedFlashParts(),
    });
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
    await connectAndFlash({
      log: appendToTerminal,
      setProgress,
      configFiles: null,
      flashParts: selectedFlashParts(),
    });
  } catch (e) {
    appendToTerminal('');
    appendToTerminal('✗ Chyba: ' + e.message);
    console.error(e);
  } finally {
    setFlashBusy(false);
  }
}

// "Iba konfig" — pre už naflashované zariadenie. Neflashuje firmware,
// iba reštartuje zariadenie cez RTS a nahraje config počas CONFIG_READY okna.
async function uploadConfigOnlyAction() {
  if (!validateConfig()) return;
  clearTerminal();
  setFlashBusy(true);
  try {
    await uploadConfigOnly({ log: appendToTerminal, setProgress, configFiles: makeConfigFiles() });
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
  onBoardChange();
  updateSecretFingerprints();

  document.addEventListener('input',  () => { saveToStorage(); updateSecretFingerprints(); });
  document.addEventListener('change', saveToStorage);

  toggleSection('general');
  toggleSection('cur1');

  // Expose functions for inline onclick handlers in HTML
  // (populateVersionDropdown already ran via onBoardChange above)
  Object.assign(window, {
    showPanel,
    toggleSection,
    onFundingChange,
    onBoardChange,
    downloadConfigZip,
    clearForm,
    connectAndFlashWithConfig,
    connectAndFlashOnly,
    uploadConfigOnlyAction,
    onVersionChange,
  });
});
