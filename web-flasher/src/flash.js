import { ESPLoader, Transport } from 'esptool-js';

const delay = ms => new Promise(r => setTimeout(r, ms));
const enc   = text => new TextEncoder().encode(text);

export function checkBrowserSupport() {
  if (!window.isSecureContext)
    return { ok: false, reason: 'Web Serial vyžaduje HTTPS alebo localhost. Otvor stránku cez https://' };
  if (!('serial' in navigator))
    return { ok: false, reason: 'Tento prehliadač nepodporuje Web Serial API. Použi Chrome alebo Edge 89+.' };
  return { ok: true };
}

// Pulse RTS low to reset the ESP32 (EN pin). Best-effort — some boards or
// browsers may not support setSignals; the user can press RESET manually.
async function pulseResetViaRTS(port, log) {
  try {
    await port.setSignals({ dataTerminalReady: false, requestToSend: true });
    await delay(120);
    await port.setSignals({ dataTerminalReady: false, requestToSend: false });
    return true;
  } catch (e) {
    if (log) log('Pozn.: auto-reset (RTS) sa nepodaril. Ak nič nepríde, stlač RESET na zariadení.');
    return false;
  }
}

export async function connectAndFlash({ log, setProgress, configFiles, flashParts }) {
  const parts = flashParts;
  if (!parts || parts.length === 0) throw new Error('Chýba zoznam firmware súborov');
  const isRemote = parts.some(p => /^https?:\/\//i.test(p.path));

  log('Vyber COM port zariadenia...');
  const port = await navigator.serial.requestPort();
  const transport = new Transport(port);

  const esploader = new ESPLoader({
    transport,
    baudrate: 115200,
    terminal: {
      clean()        {},
      writeLine(d)   { log(d); },
      write(d)       {},
    },
  });

  try {
    log('Synchronizujem s bootloaderom...');
    log('(Ak treba: drž BOOT, stlač RESET, pusť BOOT, potom pokračuj)');
    await esploader.main();
    log('Spojenie nadviazané.');

    log('');
    log(isRemote
      ? 'Sťahujem firmware z GitHub release...'
      : 'Načítavam lokálne firmware súbory...');
    const fileArray = [];
    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      let resp;
      try {
        resp = await fetch(part.path);
      } catch (e) {
        if (isRemote) {
          throw new Error('GitHub nepovoľuje sťahovanie .bin z prehliadača (CORS). '
            + 'Vyber „Najnovšia (lokálna kópia zo stránky)", alebo použi offline balík z releasu.');
        }
        throw e;
      }
      if (!resp.ok) throw new Error(`Chyba pri načítaní ${part.path}: HTTP ${resp.status}`);
      const blob = await resp.blob();
      const data = await blobToBinaryString(blob);
      fileArray.push({ data, address: part.offset });
      const label = isRemote
        ? `[${i + 1}/${parts.length}] ${part.path.split('/').pop()}`
        : `  ✓ ${part.path}`;
      log(`${label}  (${(blob.size / 1024).toFixed(0)} kB)`);
      setProgress(Math.round(((i + 1) / parts.length) * 15)); // 0–15% during download
    }

    log('');
    log('Flashujem firmware...');
    setProgress(15);
    await esploader.writeFlash({
      fileArray,
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
    });
    setProgress(90);
    log('Firmware flashovaný!');

    log('');
    log('Reštartujem zariadenie...');
    await esploader.hardReset();
    // Disconnect releases all internal stream locks and resets the baud rate.
    // We reopen the raw port at 115200 for config upload.
    await transport.disconnect();

    if (configFiles && Object.keys(configFiles).length > 0) {
      setProgress(93);
      await uploadConfig(port, configFiles, log);
      setProgress(98);
    }

    setProgress(100);
    log('');
    log('✓ Hotovo!');
    if (configFiles && Object.keys(configFiles).length > 0)
      log('  Konfigurácia bola nahratá automaticky.');
    else
      log('  Nastav konfiguráciu cez WiFi AP portál (http://192.168.4.1).');

  } finally {
    try { await transport.disconnect(); } catch (_) {}
  }
}

// Core: assumes `port` is already open at 115200. Waits up to `readyTimeoutMs`
// for CONFIG_READY, then writes each config file and waits for CONFIG_SAVED.
async function streamConfigToOpenPort(port, configFiles, log, readyTimeoutMs = 12000) {
  const decoder = new TextDecoder();
  const reader  = port.readable.getReader();
  let buf = '';
  let configReadyReceived = false;
  let configSavedReceived = false;

  let cancelTimer = setTimeout(() => reader.cancel(), readyTimeoutMs);

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;

      const text = decoder.decode(value, { stream: true });
      buf += text;

      for (const line of text.split('\n')) {
        const trimmed = line.trim();
        if (trimmed.startsWith('FIAT-HELL:')) log('  [device] ' + trimmed);
      }

      if (!configReadyReceived && buf.includes('FIAT-HELL:CONFIG_READY')) {
        configReadyReceived = true;
        clearTimeout(cancelTimer);

        log('Nahrávam konfiguráciu...');
        const writer = port.writable.getWriter();
        try {
          for (const [name, content] of Object.entries(configFiles)) {
            const line = `WRITE_CONFIG:${name}:${JSON.stringify(content)}\n`;
            await writer.write(enc(line));
            await delay(200);
            log(`  → ${name}`);
          }
          await writer.write(enc('CONFIG_DONE\n'));
        } finally {
          writer.releaseLock();
        }

        cancelTimer = setTimeout(() => reader.cancel(), 5000);
      }

      if (configReadyReceived && buf.includes('FIAT-HELL:CONFIG_SAVED')) {
        configSavedReceived = true;
        break;
      }
    }
  } finally {
    clearTimeout(cancelTimer);
    try { reader.releaseLock(); } catch (_) {}
  }

  if (!configReadyReceived) {
    log('✗ CONFIG_READY neprišiel — zariadenie sa nenaštartovalo, zlý USB port, alebo bežiaci firmware nepočúva.');
    throw new Error('CONFIG_READY timeout');
  }
  if (!configSavedReceived) {
    log('⚠ Konfigurácia odoslaná, ale CONFIG_SAVED neprišiel — skontroluj sériový monitor.');
  } else {
    log('✓ Zariadenie potvrdilo uloženie konfigurácie.');
  }
}

// Post-flash wrapper: device just rebooted from esptool's hardReset(), so we
// just need to open the port and wait for CONFIG_READY.
async function uploadConfig(port, configFiles, log) {
  log('Čakám na CONFIG_READY (zariadenie bootuje)...');
  await port.open({ baudRate: 115200 });
  try {
    await streamConfigToOpenPort(port, configFiles, log);
  } finally {
    try { await port.close(); } catch (_) {}
  }
}

// WiFi scan via the serial provisioning window: reset the device, wait for
// CONFIG_READY, send SCAN_WIFI and parse the FIAT-HELL:WIFI_LIST line.
// Requires FIAT-HELL firmware already on the device.
export async function scanWifiViaSerial({ log }) {
  log('Vyber COM port zariadenia...');
  const port = await navigator.serial.requestPort();
  try {
    await port.open({ baudRate: 115200 });
  } catch (e) {
    throw new Error('Port sa nedá otvoriť — pravdepodobne ho drží iný program '
                  + '(sériový monitor, iný tab flashera). Zavri ho a skús znova.');
  }

  try {
    log('Reštartujem zariadenie cez RTS (ak hardvér podporuje)...');
    const rtsOk = await pulseResetViaRTS(port, log);
    if (!rtsOk) log('Ak nič nepríde do 20 s, stlač RESET na zariadení.');
    log('Čakám na CONFIG_READY...');

    const decoder = new TextDecoder();
    const reader  = port.readable.getReader();
    let buf = '';
    let networks = null;
    let readySeen = false;
    let cancelTimer = setTimeout(() => reader.cancel(), 20000);

    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += decoder.decode(value, { stream: true });

        if (!readySeen && buf.includes('FIAT-HELL:CONFIG_READY')) {
          readySeen = true;
          clearTimeout(cancelTimer);
          log('Zariadenie pripravené, skenujem siete (~5 s)...');
          const writer = port.writable.getWriter();
          try { await writer.write(enc('SCAN_WIFI\n')); }
          finally { writer.releaseLock(); }
          cancelTimer = setTimeout(() => reader.cancel(), 15000);
        }

        const m = buf.match(/FIAT-HELL:WIFI_LIST:(\[.*?\])\s*\n/);
        if (m) {
          try { networks = JSON.parse(m[1]); } catch (_) { networks = []; }
          break;
        }
      }
    } finally {
      clearTimeout(cancelTimer);
      try { reader.releaseLock(); } catch (_) {}
    }

    if (!readySeen) {
      throw new Error('Zariadenie sa neohlásilo (CONFIG_READY neprišiel). '
                    + 'Skontroluj, či je v ňom FIAT-HELL firmvér a či port nedrží iný program; '
                    + 'prípadne stlač RESET na zariadení hneď po kliknutí.');
    }
    if (networks === null) {
      throw new Error('Zariadenie sa ohlásilo, ale zoznam sietí neposlalo — '
                    + 'firmvér je pravdepodobne starší bez podpory skenu. Preflashuj na aktuálnu verziu.');
    }
    log(`Nájdených sietí: ${networks.length}`);
    return networks;
  } finally {
    try { await port.close(); } catch (_) {}
  }
}

// Read the device's current configuration via the serial provisioning window.
// Secrets arrive redacted as "__SET__" (they never leave the device).
// Returns { '/first.json': parsedJson, ... }.
export async function readConfigViaSerial({ log }) {
  log('Vyber COM port zariadenia...');
  const port = await navigator.serial.requestPort();
  try {
    await port.open({ baudRate: 115200 });
  } catch (e) {
    throw new Error('Port sa nedá otvoriť — pravdepodobne ho drží iný program '
                  + '(sériový monitor, iný tab flashera). Zavri ho a skús znova.');
  }

  try {
    log('Reštartujem zariadenie cez RTS (ak hardvér podporuje)...');
    const rtsOk = await pulseResetViaRTS(port, log);
    if (!rtsOk) log('Ak nič nepríde do 20 s, stlač RESET na zariadení.');
    log('Čakám na CONFIG_READY...');

    const decoder = new TextDecoder();
    const reader  = port.readable.getReader();
    let buf = '';
    let readySeen = false;
    let dumpDone = false;
    let cancelTimer = setTimeout(() => reader.cancel(), 20000);

    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += decoder.decode(value, { stream: true });

        if (!readySeen && buf.includes('FIAT-HELL:CONFIG_READY')) {
          readySeen = true;
          clearTimeout(cancelTimer);
          log('Načítavam konfiguráciu zo zariadenia...');
          const writer = port.writable.getWriter();
          try { await writer.write(enc('READ_CONFIG\n')); }
          finally { writer.releaseLock(); }
          cancelTimer = setTimeout(() => reader.cancel(), 12000);
        }

        if (buf.includes('FIAT-HELL:CONFIG_DUMP_DONE')) { dumpDone = true; break; }
      }
    } finally {
      clearTimeout(cancelTimer);
      try { reader.releaseLock(); } catch (_) {}
    }

    if (!readySeen) {
      throw new Error('Zariadenie sa neohlásilo (CONFIG_READY neprišiel). '
                    + 'Skontroluj, či je v ňom FIAT-HELL firmvér a či port nedrží iný program.');
    }
    if (!dumpDone) {
      throw new Error('Konfigurácia neprišla — firmvér je pravdepodobne starší '
                    + 'bez podpory čítania. Preflashuj na aktuálnu verziu.');
    }

    const files = {};
    for (const m of buf.matchAll(/FIAT-HELL:CONFIG_FILE:(\/[\w.-]+\.json):(.*)\r?\n/g)) {
      try { files[m[1]] = JSON.parse(m[2]); } catch (_) { /* skip broken line */ }
    }
    log(`Načítaných súborov: ${Object.keys(files).length}`);
    return files;
  } finally {
    try { await port.close(); } catch (_) {}
  }
}

// "Config only" flow: open the port, pulse RTS to reset the chip, then run
// the same wait-and-send sequence. No flashing happens.
export async function uploadConfigOnly({ log, setProgress, configFiles }) {
  if (!configFiles || Object.keys(configFiles).length === 0)
    throw new Error('Žiadne konfiguračné údaje');

  log('Vyber COM port zariadenia...');
  const port = await navigator.serial.requestPort();

  log('Otváram port (115200)...');
  await port.open({ baudRate: 115200 });
  setProgress(10);

  try {
    log('Reštartujem zariadenie cez RTS (ak hardvér podporuje)...');
    const rtsOk = await pulseResetViaRTS(port, log);
    if (!rtsOk) log('Ak nič nepríde do 20 s, stlač RESET na zariadení.');
    log('Čakám na CONFIG_READY (~10 s okno po štarte firmware)...');
    setProgress(30);

    // 20 s — covers boot delay + manual RESET press if RTS reset didn't work
    await streamConfigToOpenPort(port, configFiles, log, 20000);
    setProgress(95);
    log('');
    log('✓ Hotovo! Konfigurácia bola nahratá.');
    setProgress(100);
  } finally {
    try { await port.close(); } catch (_) {}
  }
}

function blobToBinaryString(blob) {
  return new Promise((res, rej) => {
    const reader = new FileReader();
    reader.onloadend = () => res(reader.result);
    reader.onerror   = rej;
    reader.readAsBinaryString(blob);
  });
}
