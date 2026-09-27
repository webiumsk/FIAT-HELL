# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is FIAT-HELL

Firmware for a Bitcoin/Lightning ATM (fiat → BTC via Lightning Network). Runs on **ESP32-S3** (Sunton ESP32-8048S050C board), built with **PlatformIO + Arduino framework**. Accepts banknotes via NV10USB+, renders UI with **LVGL 8.1** on an RGB display with TAMC_GT911 touch controller. Integrates with **LNbits** (offline-capable) or **Blink** (online) as funding sources. Configuration is stored in SPIFFS (JSON files). On S3 the WiFi/config AP is handled by the in-tree `WifiPortal` module (no AutoConnect); the WT32 board still uses AutoConnect.

## Build & Upload Commands

```bash
# Build
pio run

# Build and upload to device
pio run --target upload

# Monitor serial output
pio device monitor

# Build minimal boot test env (different src filter, no LVGL)
pio run -e esp32-8048s050-minimal
```

**Upload requires manual reset:** hold BOOT, press RESET, release, then run upload. Auto-reset is disabled (`upload_use_1200bps_touch = no`). Upload speed is 460800.

## Architecture

### State Split (critical)

Two global structs are the backbone of the application:

- **`DeviceState`** (`src/DeviceState.h`) — persistent configuration: currencies, API keys (LNbits/Blink), limits, charges, bill acceptor channel mappings. Loaded/saved by `ConfigService` from SPIFFS JSON files (`/elements.json`, `/first.json`, `/second.json`, `/third.json`, `/gui.json`).
- **`SessionState`** (`src/SessionState.h`) — runtime-only state: current transaction totals, selected currency, market prices, payment flow data (QR/invoice/LNURL), UI state machine (`UiState` enum). Never persisted across reboots.

In `main.cpp`, macros alias `qrData`, `baseURLATM`, `secretATM`, etc. to `sessionState` fields — do not remove these macros as they are still referenced throughout the file.

### Main entry point

`src/main.cpp` is large and contains `setup()`, `loop()`, most business logic, and UI wiring. Key sections: bill acceptor UART handling, payment flow state machine, OTA update logic, and LVGL event callbacks.

### Services (`src/services/`)

- **`ConfigService`** — loads/saves all config from SPIFFS; populates `DeviceState`.
- **`PaymentService`** — handles LNURL-withdraw flow for LNbits and Blink invoice creation/polling.
- **`UiController`** — manages LVGL screen transitions and UI updates.

### UI Pages (`src/page*.h`)

Header-only LVGL screen definitions included directly in `main.cpp`:
- `pagefirst.h` / `pageone.h` — logo/loading and main currency selection screen
- `pagesecond.h` — money insertion screen
- `pagethird.h` — QR code / payment screen
- `pagesetup.h` — S3 config portal page served at `/setup` (plain HTML form, no AutoConnect)
- `pagegui.h` / `pageota.h` / `pagefirst.h` / `pageone.h` / `pagesecond.h` / `pagethird.h` — AutoConnect page definitions, WT32 only

### WiFi / config portal on S3 (`src/boards/s3/WifiPortal.*`)

Single owner of WiFi state. Rules that keep the config AP stable: one blocking STA attempt (12 s) always runs *before* the AP starts; while the AP is up the STA never retries on its own (retries cause scans, scans hop the AP channel and drop phones), except an explicit slow retry in recovery mode; modem sleep is off while the AP runs. Credentials live in `/wifi.json` (written by the web flasher or `/setup/save`); on first boot without that file the module migrates the last entry from AutoConnect's NVS store. Captive-portal probes (`/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, ...) and `onNotFound` reply with an absolute 302 to `http://192.168.4.1/setup`, which is what makes phones auto-open the page. Portal routes (`/setup`, `/setup/save`, `/setup/wifi-scan`, `/setup/ota`, `/setup/ota-catalog`, `/setup/ota-run`) are only served to clients on the 192.168.4.x subnet.

### Background Task

`PriceBalanceTask.cpp/.h` — FreeRTOS task that fetches BTC price (CoinGecko / ExchangeApi / CoinYEP) and wallet balance without blocking the main loop. Triggered via `triggerPriceBalanceFetch()`; result consumed via `consumePriceBalanceDataReady()`.

### Display

`SuntonDisplay.h` — wraps `Arduino_ESP32RGBPanel` + `Arduino_RGB_Display` + `TAMC_GT911` touch init. GPIO2 drives backlight (TFT_BL), DATA_G5 is on GPIO4 — both conflict with coin mech pins, so the coin mech is permanently disabled.

## Hardware Pin Constraints

- **Bill acceptor (NV10USB+):** UART1 on **GPIO 17 (TX)** and **GPIO 18 (RX)** — P3 header.
- **Do not use GPIO 32/33** — these pins do not exist on the ESP32-S3 architecture; on the Sunton ESP32-8048S050 board referencing them causes a boot loop.
- **GPIO 2** (backlight) and **GPIO 4** (RGB DATA_G5) conflict with any new peripheral on those pins.
- **Battery ADC:** GPIO 10, voltage divider ratio 5.0 (30kΩ + 7.5kΩ), 3S Li-ion range 10.5–12.6V.

## Key Constants (top of `main.cpp`)

- `FW_VERSION` — firmware version string
- `BTN1 0` — BOOT button
- `BILL_ACCEPTOR_ENABLED` — set to 0 to disable bill acceptor at compile time
- `TX2 / INHIBITMECH` — set to `-1` (coin mech disabled)

## LVGL & Font Configuration

LVGL is configured via `src/lv_conf.h` and build flags in `platformio.ini`. Custom fonts are `.c` files in `src/`. To add a new font: generate the `.c` file, add it to `src/`, enable the corresponding `LV_FONT_*` flag in `platformio.ini`.

**Do not change the LVGL version (8.1.0)** or other pinned library versions (AutoConnect 1.4.2, ArduinoJson 6.21.5, uBitcoin 0.2.x) without thorough testing — version mismatches have broken builds and runtime behavior.

## OTA Updates

OTA constants `OTA_BASE_URL` and `OTA_CATALOG_URL` are defined in `main.cpp`. The `ota-server/` directory contains a PHP server script and `.htaccess`. See `ota-server/README.md` for deployment.

## Language Note

Code comments are a mix of English and Slovak — both are acceptable when adding new comments.
