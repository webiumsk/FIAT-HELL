import { ESPLoader, Transport } from 'esptool-js';
import { FLASH_PARTS } from './config.js';

const delay = ms => new Promise(r => setTimeout(r, ms));
const enc   = text => new TextEncoder().encode(text);

export function checkBrowserSupport() {
  if (!window.isSecureContext)
    return { ok: false, reason: 'Web Serial vyžaduje HTTPS alebo localhost. Otvor stránku cez https://' };
  if (!('serial' in navigator))
    return { ok: false, reason: 'Tento prehliadač nepodporuje Web Serial API. Použi Chrome alebo Edge 89+.' };
  return { ok: true };
}

export async function connectAndFlash({ log, setProgress, configFiles }) {
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
    log('Načítavam firmware súbory...');
    const fileArray = [];
    for (const part of FLASH_PARTS) {
      const resp = await fetch(part.path);
      if (!resp.ok) throw new Error(`Chyba pri načítaní ${part.path}: HTTP ${resp.status}`);
      const blob = await resp.blob();
      const data = await blobToBinaryString(blob);
      fileArray.push({ data, address: part.offset });
      log(`  ✓ ${part.path}  (${(blob.size / 1024).toFixed(0)} kB)`);
    }

    log('');
    log('Flashujem firmware...');
    setProgress(5);
    await esploader.writeFlash({
      fileArray,
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
    });
    setProgress(90);
    log('Firmware flashovaný!');

    log('');
    log('Restartujem zariadenie...');
    await esploader.hardReset();
    // Disconnect releases all internal stream locks and resets the baud rate.
    // We reopen the raw port at 115200 for config upload.
    await transport.disconnect();

    if (configFiles && Object.keys(configFiles).length > 0) {
      setProgress(93);
      log('Čakám na CONFIG_READY...');
      // Open port immediately — device prints CONFIG_READY early in setup()
      await port.open({ baudRate: 115200 });
      try {
        await waitForConfigReady(port, 8000);
        log('Nahrávam konfiguráciu...');
        const writer = port.writable.getWriter();
        try {
          for (const [name, content] of Object.entries(configFiles)) {
            const line = `WRITE_CONFIG:${name}:${JSON.stringify(content)}\n`;
            await writer.write(enc(line));
            await delay(200);
            log(`  ✓ ${name}`);
          }
          await writer.write(enc('CONFIG_DONE\n'));
          await delay(800);
        } finally {
          writer.releaseLock();
        }
      } finally {
        try { await port.close(); } catch (_) {}
      }
      setProgress(98);
      log('Konfigurácia uložená.');
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

// Reads from port until "FIAT-HELL:CONFIG_READY" is seen or timeout expires.
// Releases the reader lock before returning so the writable side can be used.
async function waitForConfigReady(port, timeoutMs) {
  const reader = port.readable.getReader();
  const decoder = new TextDecoder();
  let buf = '';
  // reader.cancel() resolves any pending read() with done=true
  const timer = setTimeout(() => reader.cancel(), timeoutMs);
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += decoder.decode(value, { stream: true });
      if (buf.includes('FIAT-HELL:CONFIG_READY')) break;
    }
  } finally {
    clearTimeout(timer);
    try { reader.releaseLock(); } catch (_) {}
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
