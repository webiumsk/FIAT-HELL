# Inštrukcie pre AI agentov – FIAT-HELL

Tento dokument popisuje kontext projektu a pravidlá pre úpravy kódu, aby agent mohol konzistentne pracovať s repozitárom.

---

## Čo je FIAT-HELL

- **Firmvér** pre zariadenie typu Bitcoin/Lightning ATM (fiat → Bitcoin/Lightning).
- **Platforma:** ESP32-S3 (Sunton ESP32-8048S050), PlatformIO, Arduino framework.
- **UI:** LVGL 8.1 na RGB displeji, dotyk TAMC_GT911.
- **Funkcie:** prijímanie bankoviek (NV10USB+), LNbits/Blink integrácia, OTA aktualizácie, WiFi cez AutoConnect, konfigurácia cez web/SPIFFS.

---

## Štruktúra projektu

| Cesta | Účel |
|-------|------|
| `src/main.cpp` | Hlavný vstupný bod – setup, loop, väčšina logiky a UI (veľký súbor). |
| `src/DeviceState.h` | Perzistentný stav zariadenia (mena, API kľúče, limity, konfig bill acceptor). |
| `src/SessionState.h` | Stav behu session (QR, baseURL, secret pre aktuálnu menu a platbu). |
| `src/services/` | ConfigService, PaymentService, UiController – služby a čiastočná abstrakcia. |
| `src/PriceBalanceTask.*` | Úloha pre ceny a balance. |
| `src/SuntonDisplay.h` | Inicializácia a ovládanie RGB displeja. |
| `src/page*.h` | Jednotlivé obrazovky/stránky UI (pagegui, pageota, pagefirst, pageone, pagesecond, pagethird). |
| `src/lv_conf.h`, `lv_font_*.c` | Konfigurácia LVGL a fonty. |
| `platformio.ini` | Build env `esp32-8048s050`, knižnice, build_flags (LVGL, PSRAM, stack). |
| `docs/` | Dokumentácia (napr. `BILL_ACCEPTOR_NV10USB.md`). |
| `ota-server/` | Skripty/servis pre OTA (napr. PHP). |
| `boards/` | Board definície PlatformIO. |

---

## Konvencie a pravidlá

### Stav a makrá

- **DeviceState** – načítava/serializuje `ConfigService`; drží konfiguráciu (meny, LNbits, Blink, limity, bill amounts).
- **SessionState** – runtime stav pre aktuálnu session (QR, baseURL, secret pre zvolenú menu).
- V `main.cpp` sú `deviceState` a `sessionState` makrá smerujúce na globálne pointre; nepridávaj ďalšie globálne stavové premenné bez potreby a konzistentne používaj tieto štruktúry.
- Dôležité konštanty a piny sú hore v `main.cpp` (napr. `FW_VERSION`, `BTN1`, `RX1`/`TX1` pre bill acceptor, `TX2`/`INHIBITMECH` pre coin mech).

### Hardvér – piny a obmedzenia

- **Bill acceptor (NV10):** UART na **GPIO 17 (Tx)** a **GPIO 18 (Rx)** – P3 header. Na tejto doske **nepoužívaj GPIO 32/33** (spôsobujú boot loop).
- **Coin mech:** momentálne vypnutý (`TX2`/`INHIBITMECH` sú -1) kvôli konfliktu s RGB panelom (GPIO4, GPIO2).
- Pridávanie nových periférií: skontroluj konflikty s RGB a backlight v `SuntonDisplay.h` a v dokumentácii dosky.

### Kód a štýl

- **Jazyk:** kód a komentáre sú zmes angličtiny a slovenčiny; pri nových komentároch môžeš použiť ktorýkoľvek, dôležité je aby boli zrozumiteľné.
- **Pamäť:** zariadenie má obmedzenú RAM/stack; vyhni sa veľkým bufferom a hlbokej rekurzii; občas sa kontroluje `ARDUINO_LOOP_STACK_SIZE` v `platformio.ini`.
- **LVGL:** widgety a fonty sú konfigurované cez `lv_conf.h` a build flags; nové fonty pridávaj do `platformio.ini` a prípadne do `lv_conf.h`.
- **Súbory:** hlavná logika je v `main.cpp`; pri väčších zmenách zváž rozloženie do `src/` alebo `src/services/` namiesto ďalšieho rastu jedného súboru.

### Build a upload

- **Build:** `pio run` (alebo `platformio run`) v koreni projektu.
- **Env:** default je `esp32-8048s050`; pre minimálny boot test existuje `esp32-8048s050-minimal` (iný `build_src_filter`).
- **Upload:** často je vypnutý auto-reset (`upload_use_1200bps_touch = no`); manuálne: držať BOOT, stlačiť RESET, pustiť a spustiť upload. Upload speed 460800.

---

## Čo agent má robiť

- Pri zmene pinov alebo hardvéru skontrolovať `docs/BILL_ACCEPTOR_NV10USB.md` a komentáre v `main.cpp`.
- Pri úpravách konfigurácie alebo ukladania stavu brať do úvahy `DeviceState` a `ConfigService` (SPIFFS, JSON).
- Pri úpravách UI brať do úvahy LVGL a existujúce `page*.h` obrazovky.
- Pri pridávaní knižníc upraviť `platformio.ini` a otestovať build.
- Písať zmeny konzistentne s existujúcim štýlom (C++, Arduino, LVGL).

---

## Čo agent má radšej nerobiť

- Neměň bez dôvodu verziu LVGL alebo kľúčových knižníc (AutoConnect, uBitcoin, ArduinoJson) – môže to zlomiť build alebo runtime.
- Nepridávaj používanie GPIO 32/33 na tejto konkrétnej doske (Sunton ESP32-8048S050).
- Neignoruj rozdiel medzi `DeviceState` (perzistentný) a `SessionState` (runtime) – nesprávne ukladanie do toho druhého môže stratiť konfiguráciu alebo pokaziť session.
- Pri úpravách `main.cpp` neodstraňuj makrá typu `qrData`, `baseURLATM`, `secretATM` atď., ktoré mapujú na `sessionState`/`deviceState` – sú ešte používané v kóde.

---

## Užitočné odkazy v projekte

- **Bill acceptor:** `docs/BILL_ACCEPTOR_NV10USB.md`
- **OTA:** `ota-server/README.md`, konštanty `OTA_BASE_URL`, `OTA_CATALOG_URL` v `main.cpp`
- **Konfigurácia:** `ConfigService`, súbory, ako `/elements.json`, `/first.json`, `/second.json`, `/third.json`, `/gui.json` v SPIFFS

Ak potrebuješ detaily konkrétnej funkcie (napr. platby, generovanie QR, bill acceptor protokol), hľadaj v `main.cpp` a v `src/services/` podľa kľúčových slov (napr. `lnurl`, `PaymentService`, `bill acceptor`).
