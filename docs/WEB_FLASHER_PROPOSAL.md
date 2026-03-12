# Návrh: Web aplikácia pre upload firmvéru FIAT-HELL

Cieľ: Jednoduchá webová aplikácia, ktorú si používateľ otvorí v prehliadači, pripojí ESP32 cez USB, nastaví parametre a nahrá firmware. **Bez inštalácie** – len odkaz a Chrome/Edge.

---

## Technológia

### Web Serial API

- **Chrome** (Desktop, Android) a **Edge** majú [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- Safari, Firefox a iOS nemajú podporu – používateľ musí mať Chrome alebo Edge
- Je potrebný **HTTPS** – ideálne GitHub Pages

### ESP Web Tools + esptool-js

- [esphome/esp-web-tools](https://github.com/esphome/esp-web-tools) – otvorený nástroj pre flashovanie ESP v prehliadači
- Používa [esptool-js](https://github.com/espressif/esptool-js) (JavaScript verzia esptool)
- Projekty ako ESPHome, WLED, Tasmota používajú rovnaký prístup

---

## Štruktúra repozitára a hostingu

Navrhovaná štruktúra (napr. nový repozitár `FIAT-HELL-web-flasher` alebo priečinok v existujúcom):

```
fiat-hell-web-flasher/          # alebo docs/flasher/ v tomto repozite
├── index.html                  # Hlavná stránka s UI
├── manifest.json               # Manifest pre ESP Web Tools
├── bin/                        # Binárky (commitnuté alebo CI build)
│   ├── bootloader.bin
│   ├── partitions.bin
│   └── firmware.bin
└── README.md
```

**GitHub Pages:**

1. Repo: `https://github.com/<user>/FIAT-HELL-web-flasher`
2. Settings → Pages → Source: main branch, / (root)
3. URL: `https://<user>.github.io/FIAT-HELL-web-flasher/`

*Poznámka: binárky treba hostovať priamo z repa (nie GitHub Releases) kvôli CORS.*

---

## Workflow pre používateľa

1. Otvorí stránku v Chrome
2. Klikne na „Zvoliť parametre“ (voliteľné)
3. Vyplní LNbits URL, API kľúče, menu atď.
4. Pripojí ESP32 cez USB
5. Drží BOOT, stlačí RESET (ak je treba manuálny boot do flash módu)
6. Klikne na „Flashovať“
7. Vyberie COM port
8. Počka na dokončenie

---

## Konfigurácia pred uploadom

### Varianta A: Konfigurácia po prvom boote (jednoduchšia)

- Web flasher **len nahráva firmvér** (bootloader + partitions + app)
- Po prvom spustení zariadenie vytvorí WiFi AP „FIAT-HELL“
- Používateľ sa pripojí, otvorí captive portal a nastaví WiFi + ostatné parametre

**Výhody:** Jednoduchá implementácia, žiadne SPIFFS v prehliadači  
**Nevýhody:** Dva kroky – flash, potom konfigurácia cez AP

### Varianta B: Generovanie konfigu v prehliadači (lepšia UX)

- Formulár v prehliadači: LNbits URL, baseURL, secret, currency, bill amounts, max amount, charge…
- Aplikácia **vygeneruje JSON súbory** (`first.json`, `elements.json` …) podľa formátu v `ConfigService`
- Možnosti:
  1. **Stiahnutie ZIP** – používateľ stiahne ZIP s JSON súbormi a po prvom boote ich nahra cez config portál
  2. **Zapísanie do SPIFFS** – flashujem firmvér aj SPIFFS obraz s týmito JSON súbormi

Pre **zapísanie SPIFFS** v prehliadači by bolo treba:
- Vytvoriť SPIFFS image v JavaScripte (napr. použiť knižnicu ako `spiffs-gen` alebo vlastnú implementáciu)
- Do manifestu pridať ďalší „part“ so SPIFFS offsetom (0xc90000 pre default_16MB)
- Flashovať firmvér + SPIFFS v jednom behu

**Odporúčanie:** Začať s variantou A (len firmvér) a Variantou B1 (ZIP s JSON). Variantu B2 (SPIFFS) riešiť neskôr, ak bude záujem.

---

## manifest.json pre ESP32-S3

PlatformIO generuje súbory v `.pio/build/esp32-8048s050/`:
- `bootloader.bin` → offset `0x1000` (4096)
- `partitions.bin` → offset `0x8000` (32768)
- `boot_app0.bin` → offset `0xE000` (57344), ak existuje
- `firmware.bin` → offset `0x10000` (65536)

Pre ESP32-S3 sa v manifeste uvádza `chipFamily: "ESP32-S3"`.

```json
{
  "name": "FIAT-HELL ATM",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": false,
      "parts": [
        {"path": "bootloader.bin", "offset": 4096},
        {"path": "partitions.bin", "offset": 32768},
        {"path": "boot_app0.bin", "offset": 57344},
        {"path": "firmware.bin", "offset": 65536}
      ]
    }
  ]
}
```

*Poznámka: `boot_app0.bin` je pre OTA – skontrolovať, či PlatformIO ho generuje pre danú board def.*

---

## Príklad index.html

Minimálna stránka využívajúca ESP Web Tools:

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>FIAT-HELL Flasher</title>
  <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/esp-web-tools.js"></script>
</head>
<body>
  <h1>FIAT-HELL – nahratie firmvéru</h1>
  <p>Potrebný je Chrome alebo Edge. Pripojte ESP32 cez USB.</p>
  <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
</body>
</html>
```

Komponent `<esp-web-install-button>` zobrazí tlačidlo a po kliknutí spustí flashovanie. Manifest URL môže byť relatívny (rovnaká doména).

---

## Rozšírenie: formulár pre config

Ak pôjdeš cestou generovania config JSON pred/po flashe:

1. **Sekcia „Pred prvým zapnutím“** – formulár s poliami:
   - WiFi heslo (AP)
   - LNbits base URL, admin key, read key
   - Blink API key, wallet ID (ak používa Blink)
   - Mena, max sumy, poplatky
   - Bill acceptor amounts (CSV)

2. **Logika:** Po vyplnení vygenerovať `first.json`, `elements.json` atď. v konzistentnom formáte ako `ConfigService` (pole objektov s `name`, `value`, `checked` …).

3. **Výstup:**
   - Stiahnutie ZIP súboru s týmito JSON súbormi
   - Krátky návod: „Po prvom zapnutí sa pripojte na WiFi AP FIAT-HELL a nahrajte tieto súbory cez konfiguračnú stránku“

---

## Potrebné kroky pre release

1. **Build binárok** – `pio run` a skopírovanie `bootloader.bin`, `partitions.bin`, `firmware.bin`, prípadne `boot_app0.bin` do `bin/`
2. **Otestovanie** – manuálne overiť flash cez web na reálnom zariadení
3. **CI (voliteľné)** – GitHub Action, ktorý pri tagu buildne projekt a aktualizuje `bin/` v gh-pages alebo v hlavnej branch
4. **Dokumentácia** – README s odkazom na web flasher a stručným návodom

---

## Bezpečnosť a obmedzenia

- API kľúče sa spracúvajú **len v prehliadači** (localStorage / pamäť). Pri stiahnutí ZIP ide o súbory na disku používateľa.
- Žiadny backend – všetko beží v prehliadači.
- Web Serial vyžaduje používateľskú interakciu (klik na Connect).
- Na niektorých Windows treba **CH340/CP2102 driver** – stačí ho nainštalovať raz.

---

## Súhrn

| Položka | Riešenie |
|--------|----------|
| Hosting | GitHub Pages |
| Flashing | ESP Web Tools + esptool-js |
| Prehliadač | Chrome / Edge (Web Serial) |
| Config pred flashom | Formulár → ZIP s JSON (jednoduchšia cesta) alebo SPIFFS flash (pokročilá) |
| Pre používateľa | Otvorenie linku, pripojenie USB, stlačenie Flash |

Stačí jeden link, ktorý môžeš dať do README alebo na web – používateľ nemusí nič inštalovať.
