//= == == == == == == == == == == == == == == == == == == == == == == == == ==
//== == = //
//============EDIT IF USING DIFFERENT HARDWARE============//
//========================================================//
#include "version.h"

bool format = false; // true for formatting FOSSA memory, use once, then make
                     // false and reflash

#define BTN1 0 // BOOT button on ESP32-8048S050

// Sunton ESP32-8048S050: use P3 header - GPIO 18 (Rx), 17 (Tx). GPIO 32/33 cause boot loop (NA on S3).
#define RX1 18 // Bill acceptor: NV10 Pin1(Tx) -> P3 pin (GPIO18)
#define TX1 17 // Bill acceptor: NV10 Pin5(Rx) -> P3 pin (GPIO17)

#define BILL_ACCEPTOR_ENABLED 1

// GPIO4 is used by the RGB display data bus and GPIO2 drives the backlight on
// this board, so the original coin mech pins conflict with the panel wiring.
// Leave coin mech disabled until the correct non-conflicting pins are known.
#define TX2 (-1)         // Coinmech disabled: GPIO4 conflicts with LCD DATA_G5
#define INHIBITMECH (-1) // Coinmech disabled: GPIO2 conflicts with TFT backlight

// Battery indicator: ADC pin for voltage divider (12V→3.3V). Set -1 to disable.
#define BATTERY_ADC_GPIO 10
#define V_BATT_MIN 10.5f  // Empty (3S Li-ion)
#define V_BATT_MAX 12.6f  // Full (3S Li-ion)
// Voltage divider: V_batt = adc_voltage * DIVIDER_RATIO. For 30k+7.5k module: 5.0
#define BATTERY_DIVIDER_RATIO 5.0f

//========================================================//
//========================================================//
//========================================================//

#include "SuntonDisplay.h"
static SuntonDisplay lcd;

#include "lv_conf.h"
#include "lv_font_montserrat_bold_60.c"
#include "lv_font_the_bold_48.c"
#include <lvgl.h>

static const lv_color_t colors[] = {LV_COLOR_PURPLE, LV_COLOR_RED,   LV_COLOR_ORANGE,
                       LV_COLOR_YELLOW, LV_COLOR_GREEN, LV_COLOR_BLUE};

#include <FS.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include "ota/FirmwareVersion.h"
#include "ota/OtaImage.h"
#include "payout/Bolt11.h"
#include "payout/NoteMask.h"
#include "payout/OfflineLnurl.h"
#include "payout/Quote.h"
#include "portal/PortalPolicy.h"
#include "services/HttpsClient.h"
#include "services/PortalAccess.h"
#include <climits>
#include <cmath>

using WebServerClass = WebServer;
fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true

// Embedded firmware version marker. The OTA handler scans a downloaded image
// for it, so a renamed file cannot bypass the downgrade check. setup() prints
// it, which keeps the linker from discarding it.
static const char kFirmwareVersionMarker[] = "FHFW:" FW_VERSION;

static const char *resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_UNKNOWN:
    return "UNKNOWN";
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXTERNAL";
  case ESP_RST_SW:
    return "SOFTWARE";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "OTHER";
  }
}

#include <AutoConnect.h>
#include <AutoConnectCredential.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "PriceBalanceTask.h"
#define AUTOCONNECT_USE_LOG 1
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <JC_Button.h>
#include <SPI.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <Bitcoin.h>
#include <HTTPClient.h>
#include <Hash.h>

#include <iostream>
#include <algorithm>
#include <vector>

#include <cstring> // For memset

#include "btcsmall.c"
LV_IMG_DECLARE(btcSmallImg);

#include "amityage.c"
LV_IMG_DECLARE(amityImg);

#include "blink.c"
LV_IMG_DECLARE(blink);

#include "lnbits.c"
LV_IMG_DECLARE(lnbits);

#include "DeviceState.h"
#include "SessionState.h"
#include "services/ConfigService.h"
#include "services/FundingService.h"
#include "services/PaymentService.h"
#include "services/SerialConfigService.h"
#include "services/UiController.h"
// pageflashkey.h must come before the http/field alias macros below
#include "pageflashkey.h"
#include "pageota.h"

// Runtime-allocated to avoid pre-setup global constructors on ESP32.
static DeviceState *deviceStatePtr = nullptr;
static SessionState *sessionStatePtr = nullptr;
#define deviceState (*deviceStatePtr)
#define sessionState (*sessionStatePtr)

#define OTA_BASE_URL "https://fw.lnpay.eu"
#define OTA_CATALOG_URL OTA_BASE_URL "/_catalog?op=list&path="

#define PARAM_FILE "/elements.json"
#define FIRST_FILE "/first.json"
#define SECOND_FILE "/second.json"
#define THIRD_FILE "/third.json"
#define GUI_FILE "/gui.json"

// Convenience macros for compatibility (point to deviceState/sessionState)
#define qrData sessionState.qrData
// Note: password is used in config.password assignment
// Use deviceState.password directly
#define lnurl deviceState.lnurl
#define lnurl2 deviceState.lnurl2
#define lnurl3 deviceState.lnurl3
#define baseURLATM1 deviceState.baseURLATM1
#define baseURLATM sessionState.baseURLATM
#define baseURLATM2 deviceState.baseURLATM2
#define baseURLATM3 deviceState.baseURLATM3
#define secretATM1 deviceState.secretATM1
#define secretATM sessionState.secretATM
#define secretATM2 deviceState.secretATM2
#define secretATM3 deviceState.secretATM3
// Note: currencyATM, currencyATM2, currencyATM3 are used as struct field names
// Use deviceState.currencyATM, deviceState.currencyATM2,
// deviceState.currencyATM3 directly
#define currencyOne deviceState.currencyOne
#define currencyTwo deviceState.currencyTwo
#define currencyThree deviceState.currencyThree
#define currencySelected sessionState.currencySelected

lv_obj_t *burnTextLabel;

// More convenience macros
#define atmtitle deviceState.atmtitle
#define atmsubtitle deviceState.atmsubtitle
#define atmdesc deviceState.atmdesc
#define blinkapikey deviceState.blinkapikey
#define blinkwalletid deviceState.blinkwalletid
#define lnbitsURL deviceState.lnbitsURL
#define adminkey deviceState.adminkey
#define readkey deviceState.readkey
#define lnURLgen sessionState.lnURLgen
#define callback sessionState.callback
#define paymentRequest sessionState.paymentRequest
// Note: payload, boltInvoice, modifiedLnURLgen are used as local
// variables/parameters Use sessionState.payload, sessionState.boltInvoice,
// sessionState.modifiedLnURLgen directly

// Temporary buffers (remain global for now)
char totalStr[64] = {0};

// Note: fundingSourceBuffer, rateSourceBuffer, enableAnimBuffer are used in
// sizeof() and strlcpy() Use deviceState.fundingSourceBuffer,
// deviceState.rateSourceBuffer, deviceState.enableAnimBuffer directly
#define bills sessionState.bills
#define coins sessionState.coins
#define total sessionState.total
#define maxamount deviceState.maxamount
#define maxamountSelected sessionState.maxamountSelected
#define maxamount2 deviceState.maxamount2
#define maxamount3 deviceState.maxamount3
#define charge1 deviceState.charge1
#define charge2 deviceState.charge2
#define charge3 deviceState.charge3
#define chargeSelected sessionState.chargeSelected
#define fiatBalance sessionState.fiatBalance
#define fiatValue sessionState.fiatValue
#define result sessionState.result
#define isInsertingMoney sessionState.isInsertingMoney
#define previousMillis sessionState.previousMillis
#define balanceSats sessionState.balanceSats
#define initialCheck sessionState.initialCheck

// Compatibility pointers for const char* usage
const char *fundingsource = "";
const char *ratesource = "";
const char *animated = "";

// UI objects (remain global as they're LVGL objects)
lv_obj_t *balanceValueLabel = nullptr;
lv_obj_t *fiatValueLabel = nullptr;
lv_obj_t *chargeValueLabel = nullptr;
// Currency blocks on main screen (rate + fee per currency, updated by updateMainScreenLabel)
lv_obj_t *mainScreenCurrency1RateLabel = nullptr;
lv_obj_t *mainScreenCurrency1FeeLabel = nullptr;
lv_obj_t *mainScreenCurrency2RateLabel = nullptr;
lv_obj_t *mainScreenCurrency2FeeLabel = nullptr;
lv_obj_t *mainScreenCurrency3RateLabel = nullptr;
lv_obj_t *mainScreenCurrency3FeeLabel = nullptr;
lv_obj_t *mainScreenHoldLabel = nullptr;

// Temporary buffers
char buffer[32];

const long interval = 30000; // price and balance refresh, 30 seconds

// UI State Machine - now defined in SessionState.h
#define currentUiState sessionState.currentUiState
#define stateEnterTime sessionState.stateEnterTime
#define qrDebounceDone sessionState.qrDebounceDone
#define isBlinkFlow sessionState.isBlinkFlow
#define lastBlinkPollTime sessionState.lastBlinkPollTime

// Bill acceptor configuration
#define billAmountIntOne deviceState.billAmountIntOne
#define billAmountIntTwo deviceState.billAmountIntTwo
#define billAmountIntThree deviceState.billAmountIntThree
#define originalSizeOne deviceState.originalSizeOne
#define originalSizeTwo deviceState.originalSizeTwo
#define originalSizeThree deviceState.originalSizeThree

// Galoy (Blink/Flash) and LNURL-proxy endpoints live in FundingService.
const char *coinyepConversionAPI =
    "https://coinyep.com/api/v1/?from=BTC&to=";
const char *coingeckoAPI =
    "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=";
const char *exchangeapiConversionAPI =
    "https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@latest/v1/"
    "currencies/btc.json"; // https://github.com/fawazahmed0/exchange-api
const char *cuexConversionAPI = "https://api.cuex.com/v1/exchanges/btc";
const char *cuexApiKey =
    "3b71e5d431b2331acb65f2d484d423e5"; // Replace with your actual API key
const char *alternativeConversionAPI =
    "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=";

BundleTlsClient *secureClientPtr = nullptr;
HTTPClient *httpPtr = nullptr;
HardwareSerial *serialPort1Ptr = nullptr;
HardwareSerial *serialPort2Ptr = nullptr;
Button *BTNAPtr = nullptr;
#define secureClient (*secureClientPtr)
#define http (*httpPtr)
#define SerialPort1 (*serialPort1Ptr)
#define SerialPort2 (*serialPort2Ptr)
#define BTNA (*BTNAPtr)

lv_obj_t *screen_logo, *screen_portal, *screen_api, *screen_thx, *screen_main,
    *screen_insert_money, *screen_qr;
lv_obj_t *fiathell;
lv_obj_t *labelLastInserted = nullptr;
lv_obj_t *labelTotalAmount = nullptr;
lv_obj_t *labelTotalCurrency1 = nullptr;
lv_obj_t *labelTotalCurrency2 = nullptr;
lv_obj_t *labelTotalCurrency3 = nullptr;
lv_obj_t *labelTotalSats = nullptr;
lv_obj_t *labelMaxAmount = nullptr;
lv_obj_t *insertFinishBtn = nullptr;
static volatile bool insertFinishTapped = false;

lv_obj_t *loadingLabel;

/** When true, mixed limit exceeded - auto-proceed to QR without waiting for tap. */
static bool mixedLimitExceededAutoProceed = false;

static GuiConfig guiConfig;
static ConfigService configService;
static PaymentService paymentService;
static UiController *uiControllerPtr = nullptr;
#define uiController (*uiControllerPtr)

// Switch fundingsource
lv_obj_t *switch_label;
lv_obj_t *switch_fund;
lv_obj_t *rate_label;
lv_obj_t *switch_rate;
lv_obj_t *anim_label;
lv_obj_t *img_blink;
lv_obj_t *img_lnbits;

// Battery indicator (4 cells + percent label)
static lv_obj_t *battery_container = nullptr;
static lv_obj_t *battery_cells[4] = {nullptr};
static lv_obj_t *battery_label = nullptr;
static unsigned long lastBatteryUpdate = 0;
static const unsigned long BATTERY_UPDATE_INTERVAL_MS = 4000;
static int lastBatteryPct = -2; // -2 = never measured, -1 = no battery

void checkStackUsage() {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("Stack high water mark: %u bytes\n", highWaterMark);
}

#if (BATTERY_ADC_GPIO >= 0)
/**
 * @brief Read battery voltage via ADC and return percentage (0-100).
 * Returns -1 if ADC invalid or voltage out of sane range.
 */
static int readBatteryPercent() {
  // Average several samples; with no battery connected the divider input is
  // open, the pin floats, and a single read jumps all over the range.
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(BATTERY_ADC_GPIO);
    delay(1);
  }
  int raw = sum / 8;
  float vAdc = (raw / 4095.0f) * 3.3f;
  float vBatt = vAdc * BATTERY_DIVIDER_RATIO;
  if (vBatt < 8.0f || vBatt > 14.0f) {
    return -1; // Sane range check (unconnected or faulty)
  }
  float pct = (vBatt - V_BATT_MIN) / (V_BATT_MAX - V_BATT_MIN) * 100.0f;
  return (int)constrain(pct, 0, 100);
}
#endif

/**
 * @brief Create battery indicator: 4 cells + percent label in top-left.
 * Call once after first screen exists.
 */
void createBatteryIndicator() {
  if (battery_container != nullptr) return;
  battery_container = lv_obj_create(NULL);
  lv_obj_set_size(battery_container, 120, 28);
  lv_obj_set_style_bg_opa(battery_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(battery_container, LV_OPA_0, 0);
  lv_obj_set_style_pad_all(battery_container, 0, 0);
  lv_obj_clear_flag(battery_container, LV_OBJ_FLAG_SCROLLABLE);

  const int cell_w = 14;
  const int cell_h = 20;
  const int gap = 2;
  for (int i = 0; i < 4; i++) {
    battery_cells[i] = lv_obj_create(battery_container);
    lv_obj_set_size(battery_cells[i], cell_w, cell_h);
    lv_obj_set_pos(battery_cells[i], i * (cell_w + gap), 2);
    lv_obj_set_style_radius(battery_cells[i], 2, 0);
    lv_obj_set_style_border_width(battery_cells[i], 1, 0);
    lv_obj_set_style_border_color(battery_cells[i], lv_color_hex(0x606060), 0);
    lv_obj_set_style_bg_color(battery_cells[i], lv_color_hex(0x404040), 0);
    lv_obj_clear_flag(battery_cells[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  battery_label = lv_label_create(battery_container);
  lv_label_set_text(battery_label, "---");
  lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_label, lv_color_hex(0xE0E0E0), 0);
  lv_obj_set_pos(battery_label, 4 * (cell_w + gap) + 6, 4);
}

/**
 * @brief Attach battery indicator to current screen (top-left).
 * Call after each lv_scr_load().
 */
void attachBatteryToCurrentScreen() {
  if (battery_container == nullptr) return;
  lv_obj_set_parent(battery_container, lv_scr_act());
  lv_obj_align(battery_container, LV_ALIGN_TOP_LEFT, 15, 10);
#if (BATTERY_ADC_GPIO < 0)
  lv_obj_add_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
#else
  // Without a battery the divider holds the ADC near 0 V; keep the indicator
  // hidden until a battery is actually present.
  if (lastBatteryPct < 0) {
    lv_obj_add_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
  }
#endif
}

/**
 * @brief Update battery cells and percent label. Call from loop with throttling.
 */
void updateBatteryIndicator() {
  if (battery_container == nullptr || battery_cells[0] == nullptr) return;
#if (BATTERY_ADC_GPIO < 0)
  lv_obj_add_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
  return;
#endif
  int pct = readBatteryPercent();
  // A floating pin (no battery, divider input open) wanders between reads;
  // a real battery barely moves. Believe the reading only after a few
  // consecutive samples that hold still, so the indicator cannot flicker.
  static int prevPct = -2;
  static int stableStreak = 0;
  if (pct >= 0 && prevPct >= 0 && abs(pct - prevPct) <= 3) {
    stableStreak++;
  } else {
    stableStreak = 0;
  }
  prevPct = pct;
  const bool present = (pct >= 0) && (stableStreak >= 2);
  if (!present) {
    if (lastBatteryPct != -1) {
      lastBatteryPct = -1;
      lv_obj_add_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
      Serial.println("battery: no battery detected, indicator hidden");
    }
    return;
  }
  // Every label/style write invalidates the area and the resulting flush can
  // make the panel twitch; repaint only when the reading actually changed.
  if (pct == lastBatteryPct) {
    return;
  }
  if (lastBatteryPct < 0) {
    Serial.printf("battery: detected, %d%%\n", pct);
  }
  lastBatteryPct = pct;
  lv_obj_clear_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(battery_label, buf);
  int filled;
  lv_color_t color;
  if (pct >= 75) {
    filled = 4;
    color = LV_COLOR_GREEN;
  } else if (pct >= 50) {
    filled = 3;
    color = LV_COLOR_GREEN;
  } else if (pct >= 25) {
    filled = 2;
    color = LV_COLOR_ORANGE;
  } else {
    filled = 1;
    color = LV_COLOR_RED;
  }
  for (int i = 0; i < 4; i++) {
    lv_obj_set_style_bg_color(battery_cells[i],
        (i < filled) ? color : lv_color_hex(0x404040), 0);
  }
}

/* ----------------------------------
-------------- PORTAL ---------------
-----------------------------------*/

bool triggerAp = false;

static String *contentPtr = nullptr;
#define content (*contentPtr)

#include "pagefirst.h"
#include "pagegui.h"
#include "pageone.h"
#include "pagesecond.h"
#include "pagethird.h"

WebServerClass *serverPtr = nullptr;
AutoConnect *portalPtr = nullptr;
#define server (*serverPtr)
#define portal (*portalPtr)
AutoConnectConfig *configPtr = nullptr;
AutoConnectAux *elementsAuxPtr = nullptr;
AutoConnectAux *saveAuxPtr = nullptr;
AutoConnectConfig *firstPtr = nullptr;
AutoConnectAux *firstAuxPtr = nullptr;
AutoConnectAux *savefirstAuxPtr = nullptr;
AutoConnectConfig *secondPtr = nullptr;
AutoConnectAux *secondAuxPtr = nullptr;
AutoConnectAux *savesecondAuxPtr = nullptr;
AutoConnectConfig *thirdPtr = nullptr;
AutoConnectAux *thirdAuxPtr = nullptr;
AutoConnectAux *savethirdAuxPtr = nullptr;
AutoConnectConfig *guiPtr = nullptr;
AutoConnectAux *guiAuxPtr = nullptr;
AutoConnectAux *saveguiAuxPtr = nullptr;
AutoConnectAux *otaAuxPtr = nullptr;
AutoConnectAux *otaDoAuxPtr = nullptr;
#define acConfig (*configPtr)
#define elementsAux (*elementsAuxPtr)
#define saveAux (*saveAuxPtr)
#define first (*firstPtr)
#define firstAux (*firstAuxPtr)
#define savefirstAux (*savefirstAuxPtr)
#define second (*secondPtr)
#define secondAux (*secondAuxPtr)
#define savesecondAux (*savesecondAuxPtr)
#define third (*thirdPtr)
#define thirdAux (*thirdAuxPtr)
#define savethirdAux (*savethirdAuxPtr)
#define gui (*guiPtr)
#define   guiAux (*guiAuxPtr)
#define otaAux (*otaAuxPtr)
#define otaDoAux (*otaDoAuxPtr)
#define saveguiAux (*saveguiAuxPtr)

/*** Setup screen resolution for LVGL ***/
static const uint16_t screenWidth = 800;
static const uint16_t screenHeight = 480;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// Variables for touch x,y
#ifdef DRAW_ON_SCREEN
static int32_t x, y;
#endif

/*** Function declaration ***/
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p);
void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
void createLogoScreen();
void createPortalScreen();
void createAPIScreen();
void createPaymentErrorScreen();
bool createLNURLWithdraw();
bool getLNURL();
bool makeLNURL();
bool getBlinkLnURL(const char *invoice);
void createMainScreen();
void createInsertMoneyScreen();
void createSwitch(lv_obj_t *parent);
void createAcceptedCurrenciesSection();
void updateBurnText();
void updateMainScreenLabel();
void checkPrice();
void checkPriceCoinGecko();
void checkPriceExchangeApi();
bool isLNbits();
bool wifiStatus();
void showQRCodeLVGL(const char *data);
int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen,
                uint8_t *nonce, size_t nonce_len, uint64_t pin,
                uint64_t amount_in_cents);
static bool assignQuotedSats(bool mixed);
static long computeMixedMaxSats();

void checkNetworkAndDeviceStatus();
void startConfigPortal();
//void btn_reset_event_handler(lv_event_t *e);
void handleUiStateMachine();

//void createResetButton(lv_obj_t *parent);
void printHeapStatus();
void createLoadingIndicator();
void showLoadingIndicator();
void hideLoadingIndicator();
void enableAcceptor();
void completeStartupAfterPortal();
void createBatteryIndicator();
void attachBatteryToCurrentScreen();
void updateBatteryIndicator();
void reloadRuntimeConfigFromFlash();

#ifndef BOOT_DIAG_HALT_AFTER_STAGE
#define BOOT_DIAG_HALT_AFTER_STAGE 0
#endif

static void bootStage(uint8_t stage, const char *message) {
  Serial.printf("[BOOT %02u] %s\n", stage, message);
  delay(20);
  if (BOOT_DIAG_HALT_AFTER_STAGE == stage) {
    Serial.printf("[BOOT %02u] HALT_FOR_DIAG\n", stage);
    while (true) {
      delay(1000);
    }
  }
}

static void billAcceptorBegin() {
  if (BILL_ACCEPTOR_ENABLED) {
    SerialPort1.begin(300, SERIAL_8N2, RX1, TX1);  // rxPin, txPin (Arduino convention)
  }
}

// The channel mask is unknown, so the next update sends it again. It is out
// of the 16-bit range on purpose: 0xFFFF is a real mask (16 channels open),
// and mistaking it for "unknown" would skip the command that reopens them.
// A master inhibit from anywhere else (185) clears a note the mask still
// thinks is open, so the mask is marked unknown unless this write is part of
// applying the mask.
static const uint32_t kNoteMaskUnknown = 0x10000;
static uint32_t enabledNoteMask = kNoteMaskUnknown;
static bool noteMaskOwnsWrite = false;

static size_t billAcceptorWrite(uint8_t value) {
  if (value == 185 && !noteMaskOwnsWrite) {
    enabledNoteMask = kNoteMaskUnknown;
  }
  if (BILL_ACCEPTOR_ENABLED) {
    return SerialPort1.write(value);
  }
  return 0;
}

static int billAcceptorRead() {
  if (BILL_ACCEPTOR_ENABLED && SerialPort1.available()) {
    return SerialPort1.read();
  }
  return -1;
}

static bool suspendTouchPolling = false;

// Own captive-portal DNS: the only resolver on UDP 53 while the AP is up.
// We answer every query with the softAP IP. The Arduino DNSServer that
// AutoConnect would otherwise start mangles EDNS0 queries (modern phones send
// them) into malformed replies, so this is a minimal hand-rolled responder
// that drops the additional/OPT section and returns a valid A answer.
// AutoConnect's own DNSServer is suppressed via portal.onDetect() in setup().
static WiFiUDP apDnsUdp;
static bool apDnsRunning = false;

static void ensureApDns() {
  if (apDnsRunning) return;
  if (!(WiFi.getMode() & WIFI_AP)) return;
  if (apDnsUdp.begin(53)) {
    apDnsRunning = true;
    Serial.println("Captive DNS up -> " + WiFi.softAPIP().toString());
  }
}

static void stopApDns() {
  if (!apDnsRunning) return;
  apDnsUdp.stop();
  apDnsRunning = false;
}

// Handle one pending DNS query, if any. Returns true if a packet was serviced.
static bool processApDnsOnce() {
  const int avail = apDnsUdp.parsePacket();
  if (avail <= 0) return false;

  static uint8_t buf[512];
  int n = apDnsUdp.read(buf, sizeof(buf));
  if (n < 12) return true;              // too short to be a query
  if (buf[2] & 0x80) return true;       // already a response, ignore

  // Walk the first question: QNAME (labels) then QTYPE(2) + QCLASS(2).
  int p = 12;
  while (p < n && buf[p] != 0) {
    if ((buf[p] & 0xC0) == 0xC0) { p += 2; goto have_end; } // compressed
    p += buf[p] + 1;
  }
  if (p >= n) return true;
  p += 1; // null label
have_end:
  if (p + 4 > n) return true;
  const uint16_t qtype = (buf[p] << 8) | buf[p + 1];
  const int qend = p + 4; // end of question section

  // Response header: QR=1, AA=1, keep RD; RCODE=0.
  buf[2] = 0x84 | (buf[2] & 0x01);
  buf[3] = 0x00;
  buf[6] = 0x00; buf[8] = 0x00; buf[10] = 0x00; buf[11] = 0x00; // NS/AR=0
  // Answer only A queries with an address; anything else -> valid empty answer
  // (NODATA) so EDNS/AAAA probes still get a well-formed reply.
  const bool answerA = (qtype == 1);
  buf[7] = answerA ? 0x01 : 0x00; // ANCOUNT

  int outLen = qend;
  if (answerA && qend + 16 <= (int)sizeof(buf)) {
    uint8_t *a = buf + qend;
    a[0] = 0xC0; a[1] = 0x0C;             // name -> pointer to question
    a[2] = 0x00; a[3] = 0x01;             // TYPE A
    a[4] = 0x00; a[5] = 0x01;             // CLASS IN
    a[6] = 0; a[7] = 0; a[8] = 0; a[9] = 30; // TTL 30s
    a[10] = 0x00; a[11] = 0x04;           // RDLENGTH 4
    const IPAddress ip = WiFi.softAPIP();
    a[12] = ip[0]; a[13] = ip[1]; a[14] = ip[2]; a[15] = ip[3];
    outLen = qend + 16;
  }

  apDnsUdp.beginPacket(apDnsUdp.remoteIP(), apDnsUdp.remotePort());
  apDnsUdp.write(buf, outLen);
  apDnsUdp.endPacket();
  return true;
}

// Drain a small burst of queries per loop so bursts aren't dropped.
static void processApDns() {
  for (int i = 0; i < 6 && processApDnsOnce(); i++) {
  }
}
static bool pendingPortalCompletion = false;
// The startup TLS checks must not run inside setup(): that frame plus the
// handshake overflows the loop task and the ATM reboot-loops, so fiathell.local
// never answers. loop() runs them after setup() has returned.
static bool pendingPreflight = false;
static bool appStartupCompleted = false;
static bool portalRequestedByUser = false;
static bool portalRequiredForMissingConfig = false;
static bool portalRequiredForWifiRecovery = false;
static bool portalNetworkStateLogged = false;
static bool pendingConfigReload = false;
static unsigned long pendingRestartAt = 0;
// Random per-boot hotspot PSK, set only while the stored portal password
// fails the policy. Shown on the portal screen so the operator can join.
static String g_fallbackApPsk;

// The hotspot passphrase and the web login ("admin") are one secret. A stored
// password that fails the policy is replaced by a random per-boot one, and it
// stays in force until a valid password is saved. Boot and every config
// reload go through here, so a reload cannot put the rejected default back.
static void applyPortalCredentials() {
  if (portalPasswordAccepted(deviceState.password)) {
    g_fallbackApPsk = "";
    acConfig.psk = String(deviceState.password);
  } else {
    if (g_fallbackApPsk.length() == 0) {
      g_fallbackApPsk = makeFallbackApPsk();
    }
    acConfig.psk = g_fallbackApPsk;
  }
  acConfig.password = acConfig.psk;
}

// AutoConnect::isPortalAvailable() only reports its own DNSServer, which is
// suppressed here, so the config portal counts as up while the softAP serves
// clients and the station link is down.
static bool configPortalUp() {
  return (WiFi.getMode() & WIFI_AP) && !wifiStatus();
}

static void onWifiDiagEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_AP_START:
    Serial.println("[WIFI] AP started");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    Serial.println("[WIFI] AP stopped");
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
    const uint8_t *m = info.wifi_ap_staconnected.mac;
    Serial.printf("[WIFI] AP client joined %02x:%02x:%02x:%02x:%02x:%02x aid=%u\n",
                  m[0], m[1], m[2], m[3], m[4], m[5],
                  info.wifi_ap_staconnected.aid);
    break;
  }
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
    const uint8_t *m = info.wifi_ap_stadisconnected.mac;
    Serial.printf("[WIFI] AP client left %02x:%02x:%02x:%02x:%02x:%02x aid=%u\n",
                  m[0], m[1], m[2], m[3], m[4], m[5],
                  info.wifi_ap_stadisconnected.aid);
    break;
  }
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    Serial.println("[WIFI] STA connected");
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.printf("[WIFI] STA disconnected reason=%u\n",
                  info.wifi_sta_disconnected.reason);
    break;
  default:
    break;
  }
}

// Periodic radio/heap snapshot while the config portal is up, so AP drops
// can be told apart from client-side (phone) disconnects in the serial log.
// Counter reads only: heap_caps_get_largest_free_block() walks the heap with
// interrupts off and trips the Interrupt WDT here (see printHeapStatus()).
static void logPortalHeartbeat() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog < 10000UL) return;
  lastLog = millis();
  Serial.printf("[AP] clients=%u ch=%d mode=%d heapInt=%u minInt=%u psram=%u\n",
                WiFi.softAPgetStationNum(), WiFi.channel(),
                static_cast<int>(WiFi.getMode()),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// ---------------------------------------------------------------------------
// Boot self-check ("preflight"). The main screen stays unreachable until
// every blocking check passes: complete config, network (mandatory for
// Blink/Flash, warning-only offline mode for LNbits), a real BTC price for
// every configured currency, a reachable funding service whose API key may
// actually pay, and a bill acceptor answering on UART. On failure the screen
// names the broken check and offers RETRY (re-run) and SETUP (reboot into
// the config hotspot).
// ---------------------------------------------------------------------------

enum PreflightRow {
  PF_CONFIG = 0,
  PF_WIFI,
  PF_PRICE,
  PF_FUNDING,
  PF_ACCEPTOR,
  PF_ROWS
};

static lv_obj_t *preflightRowLabel[PF_ROWS] = {nullptr};
static lv_obj_t *preflightDetailLabel = nullptr;
static lv_obj_t *preflightRetryBtn = nullptr;
static lv_obj_t *preflightSetupBtn = nullptr;
static char preflightRowName[PF_ROWS][40];
static volatile bool preflightRetryTapped = false;
static volatile bool preflightSetupTapped = false;

static void preflightRetryCb(lv_event_t *) { preflightRetryTapped = true; }
static void preflightSetupCb(lv_event_t *) { preflightSetupTapped = true; }

// Same orange as the FINISH button on the bill insert screen.
static const uint32_t kActionColor = 0xF5A623;
static const uint32_t kActionTextColor = 0x1A1A1A;

static void styleActionButton(lv_obj_t *btn) {
  const lv_color_t bg = lv_color_hex(kActionColor);
  const lv_color_t text = lv_color_hex(kActionTextColor);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xC4841C),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(btn, text, LV_PART_MAIN);
  lv_obj_set_style_text_color(btn, text, LV_PART_MAIN | LV_STATE_PRESSED);
  const uint32_t n = lv_obj_get_child_cnt(btn);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_set_style_text_color(lv_obj_get_child(btn, i), text, 0);
  }
}

static void styleWaitSpinner(lv_obj_t *spinner) {
  const lv_color_t accent = lv_color_hex(kActionColor);
  lv_obj_set_style_arc_color(spinner, lv_color_hex(0x3D2A10), LV_PART_MAIN);
  lv_obj_set_style_arc_color(spinner, accent, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(spinner, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(spinner, 10, LV_PART_MAIN);
}

static void preflightSetRow(PreflightRow row, const char *status,
                            lv_color_t color) {
  if (preflightRowLabel[row] == nullptr) {
    return;
  }
  char line[96];
  snprintf(line, sizeof(line), "%s: %s", preflightRowName[row], status);
  lv_label_set_text(preflightRowLabel[row], line);
  lv_obj_set_style_text_color(preflightRowLabel[row], color, 0);
  Serial.printf("preflight[%s]: %s\n", preflightRowName[row], status);
  lv_task_handler();
}

// One line under the logo while boot blocks (loading settings, joining
// Wi-Fi). The join can take many seconds with no LVGL tick, so the text is
// the only sign of life; it is drawn once before the block starts.
static lv_obj_t *logoStatusLabel = nullptr;
static lv_obj_t *logoStatusScreen = nullptr;

static void setBootStatus(const char *text) {
  if (screen_logo == nullptr || lv_scr_act() != screen_logo) {
    return;
  }
  if (logoStatusLabel == nullptr || logoStatusScreen != screen_logo) {
    logoStatusLabel = lv_label_create(screen_logo);
    logoStatusScreen = screen_logo;
    lv_obj_set_style_text_font(logoStatusLabel, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(logoStatusLabel, lv_color_hex(kActionColor), 0);
    lv_obj_align(logoStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -40);
  }
  lv_label_set_text(logoStatusLabel, text);
  lv_obj_align(logoStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_refr_now(NULL);
}

static void createPreflightScreen() {
  lv_obj_t *screen = lv_obj_create(NULL);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "STARTUP CHECK");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, LV_COLOR_PURPLE, 0);

  strlcpy(preflightRowName[PF_CONFIG], "Configuration",
          sizeof(preflightRowName[PF_CONFIG]));
  strlcpy(preflightRowName[PF_WIFI], "WiFi network",
          sizeof(preflightRowName[PF_WIFI]));
  strlcpy(preflightRowName[PF_PRICE], "BTC price feed",
          sizeof(preflightRowName[PF_PRICE]));
  snprintf(preflightRowName[PF_FUNDING], sizeof(preflightRowName[PF_FUNDING]),
           "Funding (%s)", deviceState.fundingSourceBuffer[0] != '\0'
                               ? deviceState.fundingSourceBuffer
                               : "not set");
  strlcpy(preflightRowName[PF_ACCEPTOR], "Bill acceptor",
          sizeof(preflightRowName[PF_ACCEPTOR]));

  for (int i = 0; i < PF_ROWS; i++) {
    preflightRowLabel[i] = lv_label_create(screen);
    char line[64];
    snprintf(line, sizeof(line), "%s: ...", preflightRowName[i]);
    lv_label_set_text(preflightRowLabel[i], line);
    lv_obj_align(preflightRowLabel[i], LV_ALIGN_TOP_LEFT, 60, 100 + i * 42);
    lv_obj_set_style_text_font(preflightRowLabel[i], &lv_font_montserrat_20,
                               0);
  }

  preflightDetailLabel = lv_label_create(screen);
  lv_label_set_text(preflightDetailLabel, "");
  lv_obj_set_width(preflightDetailLabel, 720);
  lv_label_set_long_mode(preflightDetailLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(preflightDetailLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(preflightDetailLabel, LV_ALIGN_TOP_MID, 0, 330);
  lv_obj_set_style_text_font(preflightDetailLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(preflightDetailLabel, LV_COLOR_RED, 0);

  preflightRetryBtn = lv_btn_create(screen);
  lv_obj_set_size(preflightRetryBtn, 220, 64);
  lv_obj_align(preflightRetryBtn, LV_ALIGN_BOTTOM_LEFT, 120, -30);
  lv_obj_add_event_cb(preflightRetryBtn, preflightRetryCb, LV_EVENT_CLICKED,
                      nullptr);
  lv_obj_t *retryLabel = lv_label_create(preflightRetryBtn);
  lv_label_set_text(retryLabel, "RETRY");
  lv_obj_set_style_text_font(retryLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(retryLabel);
  styleActionButton(preflightRetryBtn);

  preflightSetupBtn = lv_btn_create(screen);
  lv_obj_set_size(preflightSetupBtn, 220, 64);
  lv_obj_align(preflightSetupBtn, LV_ALIGN_BOTTOM_RIGHT, -120, -30);
  lv_obj_add_event_cb(preflightSetupBtn, preflightSetupCb, LV_EVENT_CLICKED,
                      nullptr);
  lv_obj_t *setupLabel = lv_label_create(preflightSetupBtn);
  lv_label_set_text(setupLabel, "SETUP");
  lv_obj_set_style_text_font(setupLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(setupLabel);
  styleActionButton(preflightSetupBtn);

  lv_obj_add_flag(preflightRetryBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(preflightSetupBtn, LV_OBJ_FLAG_HIDDEN);

  // The rows are filled in by preflightSetRow() before the first draw, so the
  // screen is loaded once, not empty and then again with the results.
  lv_scr_load(screen);
}

// The NV10 echoes command bytes; an inhibit command is the safest probe
// because it leaves the acceptor blocked regardless of the outcome.
static bool probeBillAcceptor() {
  while (billAcceptorRead() != -1) {
  }
  billAcceptorWrite(185);
  const unsigned long start = millis();
  while (millis() - start < 2500) {
    if (billAcceptorRead() != -1) {
      return true;
    }
    lv_task_handler();
    delay(5);
  }
  return false;
}

static int smallestConfiguredNote();
static long insertSessionCap();

// Stablesats and Flash cash balances are dollar cents. The BTC formula
// (cents / 1e8 * price) paints a fake PYG number until the BTC/USD rate
// arrives, which is what the main screen used to show for the first 30 s.
static bool fundingWalletIsUsd() {
  const char *walletCur = FundingService::galoyWalletCurrency(
      deviceState.fundingSourceBuffer, deviceState.blinkWalletBuffer);
  return FundingService::isGaloy(deviceState.fundingSourceBuffer) &&
         strcmp(walletCur, "USD") == 0;
}

static bool displayedFiatBalance(double *out) {
  const float price =
      fiatValue > 0.0f ? fiatValue : sessionState.fiatValue1;
  if (out == nullptr || balanceSats <= 0 || price <= 0.0f) {
    return false;
  }
  if (fundingWalletIsUsd()) {
    if (sessionState.btcUsdValue <= 0.0f) {
      return false;
    }
    *out = ((double)balanceSats / 100.0) * (price / sessionState.btcUsdValue);
    return true;
  }
  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    *out = ((double)balanceSats * price) / 100000000000.0;
    return true;
  }
  *out = ((double)balanceSats / 100000000.0) * price;
  return true;
}

// One pass over all checks. Every check runs even after a failure so the
// operator sees everything that is broken at once. Returns false when any
// blocking check failed; firstFailure then describes the first one.
static bool runPreflightOnce(char *firstFailure, size_t firstFailureLen,
                             bool *anyWarning) {
  firstFailure[0] = '\0';
  *anyWarning = false;
  bool pass = true;
  const lv_color_t orange = lv_color_hex(0xFFA500);
  const lv_color_t gray = lv_color_hex(0x888888);
  const char *funding = deviceState.fundingSourceBuffer;
  const bool galoy = FundingService::isGaloy(funding);
  const bool lnbits = strcmp(funding, "LNbits") == 0;

  // 1. Configuration
  const char *cfgProblem = nullptr;
  if (!galoy && !lnbits) {
    cfgProblem = "funding source not set";
  } else if (galoy && blinkapikey[0] == '\0') {
    cfgProblem = "Blink API key missing";
  } else if (lnbits && (baseURLATM1[0] == '\0' || adminkey[0] == '\0' ||
                        readkey[0] == '\0')) {
    cfgProblem = "LNbits URL or keys missing";
  } else if (currencyOne[0] == '\0') {
    cfgProblem = "no currency configured";
  } else if (billAmountIntOne.empty() && billAmountIntTwo.empty() &&
             billAmountIntThree.empty()) {
    cfgProblem = "no bill channels configured";
  }
  if (cfgProblem != nullptr) {
    preflightSetRow(PF_CONFIG, cfgProblem, LV_COLOR_RED);
    strlcpy(firstFailure, cfgProblem, firstFailureLen);
    pass = false;
  } else {
    preflightSetRow(PF_CONFIG, "OK", LV_COLOR_GREEN);
  }

  // 2. WiFi. Blink/Flash cannot work offline; LNbits falls back to its
  // locally encoded LNURL with a visible warning.
  const bool wifi = wifiStatus();
  if (wifi) {
    preflightSetRow(PF_WIFI, "OK", LV_COLOR_GREEN);
  } else if (galoy) {
    preflightSetRow(PF_WIFI, "FAILED - Blink/Flash needs internet",
                    LV_COLOR_RED);
    if (firstFailure[0] == '\0') {
      strlcpy(firstFailure, "WiFi not connected (required for Blink/Flash)",
              firstFailureLen);
    }
    pass = false;
  } else {
    preflightSetRow(PF_WIFI, "offline mode", orange);
    *anyWarning = true;
  }

  // 3. BTC price for every configured currency. Without a price no quote can
  // succeed and the acceptor would stay dark, so this is blocking.
  if (!wifi) {
    preflightSetRow(PF_PRICE, "skipped (offline)", gray);
  } else {
    char priceProblem[80] = "";
    if (originalSizeOne > 0) {
      float v = 0.0f;
      if (priceBalanceFetchPriceNow(currencyOne, deviceState.rateSourceBuffer,
                                    &v)) {
        sessionState.fiatValue1 = v;
      } else {
        snprintf(priceProblem, sizeof(priceProblem), "no BTC price for %s",
                 currencyOne);
      }
    }
    if (priceProblem[0] == '\0' && originalSizeTwo > 0) {
      float v = 0.0f;
      if (priceBalanceFetchPriceNow(currencyTwo, deviceState.rateSourceBuffer,
                                    &v)) {
        sessionState.fiatValue2 = v;
      } else {
        snprintf(priceProblem, sizeof(priceProblem), "no BTC price for %s",
                 currencyTwo);
      }
    }
    if (priceProblem[0] == '\0' && originalSizeThree > 0) {
      float v = 0.0f;
      if (priceBalanceFetchPriceNow(currencyThree,
                                    deviceState.rateSourceBuffer, &v)) {
        sessionState.fiatValue3 = v;
      } else {
        snprintf(priceProblem, sizeof(priceProblem), "no BTC price for %s",
                 currencyThree);
      }
    }
    if (priceProblem[0] == '\0' && fundingWalletIsUsd()) {
      float usd = 0.0f;
      if (priceBalanceFetchPriceNow("USD", deviceState.rateSourceBuffer,
                                    &usd)) {
        sessionState.btcUsdValue = usd;
      } else {
        snprintf(priceProblem, sizeof(priceProblem), "no BTC/USD rate");
      }
    }
    if (priceProblem[0] != '\0') {
      preflightSetRow(PF_PRICE, priceProblem, LV_COLOR_RED);
      if (firstFailure[0] == '\0') {
        strlcpy(firstFailure, priceProblem, firstFailureLen);
      }
      pass = false;
    } else {
      preflightSetRow(PF_PRICE, "OK", LV_COLOR_GREEN);
      // Mirror the selected currency into fiatValue so the very first quote
      // works before the background task's first refresh.
      if (strcmp(currencySelected, currencyOne) == 0) {
        fiatValue = sessionState.fiatValue1;
      } else if (strcmp(currencySelected, currencyTwo) == 0) {
        fiatValue = sessionState.fiatValue2;
      } else if (strcmp(currencySelected, currencyThree) == 0) {
        fiatValue = sessionState.fiatValue3;
      }
    }
  }

  // 4. Funding service: reachable, key may pay, wallet can pay.
  if (!wifi) {
    preflightSetRow(PF_FUNDING, "skipped (offline)", gray);
  } else if (galoy) {
    char fundProblem[120] = "";
    if (!FundingService::fetchGaloyBalance(
            deviceState, sessionState,
            FundingService::galoyWalletCurrency(
                funding, deviceState.blinkWalletBuffer))) {
      strlcpy(fundProblem, "unreachable or API key rejected",
              sizeof(fundProblem));
    } else if (balanceSats <= 0) {
      strlcpy(fundProblem, "funding wallet is empty", sizeof(fundProblem));
    } else {
      char scopes[64] = "";
      if (!FundingService::fetchGaloyAuthorization(deviceState, scopes,
                                                   sizeof(scopes))) {
        strlcpy(fundProblem, "cannot verify API key permissions",
                sizeof(fundProblem));
      } else if (strstr(scopes, "WRITE") == nullptr) {
        snprintf(fundProblem, sizeof(fundProblem),
                 "API key cannot pay (scopes: %s)",
                 scopes[0] ? scopes : "none");
      }
    }
    if (fundProblem[0] == '\0') {
      double shown = 0.0;
      chargeSelected = charge1;
      maxamountSelected = maxamount;
      const long usable = insertSessionCap();
      const int smallestNote = smallestConfiguredNote();
      if (!displayedFiatBalance(&shown)) {
        strlcpy(fundProblem, "balance could not be priced",
                sizeof(fundProblem));
      } else if (smallestNote > 0 && usable < (long)smallestNote) {
        snprintf(fundProblem, sizeof(fundProblem),
                 "balance %ld %s, need %d", usable, currencyOne, smallestNote);
      } else {
        fiatBalance = shown;
      }
    }
    if (fundProblem[0] != '\0') {
      preflightSetRow(PF_FUNDING, fundProblem, LV_COLOR_RED);
      if (firstFailure[0] == '\0') {
        strlcpy(firstFailure, fundProblem, firstFailureLen);
      }
      pass = false;
    } else {
      preflightSetRow(PF_FUNDING, "OK", LV_COLOR_GREEN);
    }
  } else {
    long lnbitsBalance = 0;
    if (FundingService::checkLNbitsWallet(deviceState, &lnbitsBalance)) {
      balanceSats = lnbitsBalance;
      double shown = 0.0;
      chargeSelected = charge1;
      maxamountSelected = maxamount;
      const long usable = insertSessionCap();
      const int smallestNote = smallestConfiguredNote();
      if (wifi && displayedFiatBalance(&shown) && smallestNote > 0 &&
          usable < (long)smallestNote) {
        char low[120];
        snprintf(low, sizeof(low), "balance %ld %s, need %d", usable,
                 currencyOne, smallestNote);
        preflightSetRow(PF_FUNDING, low, LV_COLOR_RED);
        if (firstFailure[0] == '\0') {
          strlcpy(firstFailure, low, firstFailureLen);
        }
        pass = false;
      } else {
        if (shown > 0.0) {
          fiatBalance = shown;
        }
        preflightSetRow(PF_FUNDING, "OK", LV_COLOR_GREEN);
      }
    } else {
      preflightSetRow(PF_FUNDING, "wallet unreachable or read key rejected",
                      LV_COLOR_RED);
      if (firstFailure[0] == '\0') {
        strlcpy(firstFailure, "LNbits wallet unreachable",
                firstFailureLen);
      }
      pass = false;
    }
  }

  // 5. Bill acceptor hardware probe.
#if BILL_ACCEPTOR_ENABLED
  if (probeBillAcceptor()) {
    preflightSetRow(PF_ACCEPTOR, "OK", LV_COLOR_GREEN);
  } else {
    preflightSetRow(PF_ACCEPTOR, "no answer on UART (GPIO17/18)",
                    LV_COLOR_RED);
    if (firstFailure[0] == '\0') {
      strlcpy(firstFailure, "Bill acceptor not responding", firstFailureLen);
    }
    pass = false;
  }
#else
  preflightSetRow(PF_ACCEPTOR, "disabled in build", gray);
#endif

  return pass;
}

static void preflightGate() {
  createPreflightScreen();
  char reason[160];
  bool warned = false;
  for (;;) {
    if (runPreflightOnce(reason, sizeof(reason), &warned)) {
      if (warned) {
        lv_label_set_text(preflightDetailLabel,
                          "Offline mode: payout uses the stored LNbits link.");
        lv_obj_set_style_text_color(preflightDetailLabel,
                                    lv_color_hex(0xFFA500), 0);
        lv_task_handler();
        delay(3000);
      }
      return;
    }
    Serial.printf("preflight FAILED: %s\n", reason);
    lv_label_set_text(preflightDetailLabel, reason);
    lv_obj_clear_flag(preflightRetryBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(preflightSetupBtn, LV_OBJ_FLAG_HIDDEN);
    lv_task_handler();
    preflightRetryTapped = false;
    preflightSetupTapped = false;
    // This screen used to spin here without serving HTTP, so fiathell.local
    // sat until the browser gave up. Keep the portal alive while the
    // operator reads the failure.
    while (!preflightRetryTapped && !preflightSetupTapped) {
      portal.handleClient();
      lv_task_handler();
      delay(5);
    }
    if (preflightSetupTapped) {
      File flag = FlashFS.open("/force-portal", "w");
      if (flag) {
        flag.print("1");
        flag.close();
      }
      ESP.restart();
    }
    lv_obj_add_flag(preflightRetryBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(preflightSetupBtn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(preflightDetailLabel, "");
    lv_task_handler();
  }
}

// fiathell.local must advertise only the address clients can actually use.
// Announcing the hotspot and the home LAN together makes browsers try the
// unreachable one first and the page sits for several seconds.
static bool g_mdnsUp = false;

static void startFiathellMdns() {
  // Advertise a link-local IPv6 address. Without an AAAA answer, macOS and
  // phones wait out the IPv6 query (~5 s) before using IPv4, which is the
  // "slow then fast" page load. The address is only used for the name lookup.
  if (WiFi.getMode() & WIFI_STA) {
    WiFi.enableIpV6();
  }
  if (WiFi.getMode() & WIFI_AP) {
    WiFi.softAPenableIpV6();
  }
  if (g_mdnsUp) {
    MDNS.end();
    g_mdnsUp = false;
  }
  if (!MDNS.begin("fiathell")) {
    Serial.println("mDNS: fiathell.local failed");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  g_mdnsUp = true;
  Serial.println("mDNS: http://fiathell.local");
}

// Settings are served on the hotspot. Leave station mode joined as well and
// fiathell.local answers with both IPs; a phone on the hotspot then waits on
// the home-LAN address. One radio mode, one address.
static void settleSetupHotspot() {
  stopApDns();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(acConfig.apid.c_str(), acConfig.psk.c_str(), acConfig.channel);
  WiFi.setSleep(false);
  acConfig.autoReconnect = false;
  acConfig.reconnectInterval = 0;
  acConfig.preserveAPMode = true;
  portal.config(acConfig);
  ensureApDns();
  startFiathellMdns();
  Serial.println("Setup hotspot only: http://192.168.4.1 and http://fiathell.local");
}

// The startup check already fetched the price and the wallet balance. Copy
// them into the fields the main screen reads, so the first paint is not a
// row of zeros waiting on the background task. The center rate uses
// fiatValue1 directly; the bottom price uses fiatValue, which used to stay
// 0 until that later refresh.
static void applyDisplayedQuotes() {
  if (currencyOne[0] != '\0') {
    strlcpy(currencySelected, currencyOne, sizeof(currencySelected));
    if (sessionState.fiatValue1 > 0.0f) {
      fiatValue = sessionState.fiatValue1;
    }
    chargeSelected = charge1;
    maxamountSelected = maxamount;
  }
  double shown = 0.0;
  if (displayedFiatBalance(&shown)) {
    fiatBalance = shown;
  }
}

static void finishStartupAfterPreflight() {
  Serial.println("Proceeding to main screen");
  applyDisplayedQuotes();
  createMainScreen();
  lv_task_handler();
  bootStage(50, "main screen created");

  Serial.print("ESP Free heap (Setup end): ");
  Serial.println(ESP.getFreeHeap());

  startPriceBalanceTask(deviceStatePtr, sessionStatePtr);
  // No immediate fetch: the startup check fetched price and balance seconds
  // ago, and a TLS burst right as the main screen appears makes the panel
  // glitch. The 30 s cadence in loop() refreshes soon enough.

  appStartupCompleted = true;
  pendingPortalCompletion = false;
}

void completeStartupAfterPortal() {
  if (appStartupCompleted || pendingPreflight) {
    return;
  }

  // Portal mode turns reconnects off to keep the AP on one channel; in normal
  // operation the ATM has to recover from router outages by itself.
  acConfig.autoReconnect = true;
  acConfig.reconnectInterval = 1;
  portal.config(acConfig);
  WiFi.setAutoReconnect(true);

  if (wifiStatus() && (WiFi.getMode() & WIFI_AP)) {
    Serial.println("WiFi up; turning the setup hotspot off");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  // A fresh STA start applies modem sleep from the core default unless this
  // is repeated, and mDNS has to be published after the hotspot is gone.
  WiFi.setSleep(false);
  if (wifiStatus()) {
    startFiathellMdns();
  }

  // Extract "https://your.lnbits.com" from baseURLATM
  // "https://your.lnbits.com/lnurldevice/api/v1/lnurl/<id>";
  int thirdSlash = 0;
  int count = 0;

  for (int i = 0; i < strlen(baseURLATM); i++) {
    if (baseURLATM[i] == '/') {
      count++;
      if (count == 3) {
        thirdSlash = i;
        break;
      }
    }
  }

  if (thirdSlash > 0 && thirdSlash < sizeof(lnbitsURL)) {
    strncpy(lnbitsURL, baseURLATM, thirdSlash);
    lnbitsURL[thirdSlash] = '\0';
  } else {
    strlcpy(lnbitsURL, baseURLATM, sizeof(lnbitsURL));
  }

  Serial.print(F("lnbitsURL: "));
  Serial.println(lnbitsURL);

  // Run from loop(), once setup()'s frame is gone. See pendingPreflight.
  pendingPreflight = true;
}

void reloadRuntimeConfigFromFlash() {
  Serial.println("Reloading runtime config from FlashFS");

  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile) {
    DynamicJsonDocument conf(2400);
    DeserializationError error = deserializeJson(conf, paramFile);
    if (!error) {
      const char *conf0Char = conf[0]["value"] | "changeme";
      const char *conf1Char = conf[1]["value"] | "";
      const char *conf2Char = conf[2]["value"] | "";
      const char *conf3Char = conf[3]["value"] | "FIAT HELL";

      strlcpy(deviceState.password, conf0Char, sizeof(deviceState.password));
      strlcpy(atmdesc, conf1Char, sizeof(atmdesc));
      strlcpy(atmsubtitle, conf2Char, sizeof(atmsubtitle));
      strlcpy(atmtitle, conf3Char, sizeof(atmtitle));
    } else {
      Serial.print("Reload parse failed for ");
      Serial.print(PARAM_FILE);
      Serial.print(": ");
      Serial.println(error.c_str());
    }
    paramFile.close();
  }

  FirstConfig firstCfg;
  if (configService.loadFirst(FlashFS, FIRST_FILE, firstCfg)) {
    strlcpy(blinkapikey, firstCfg.blinkApiKey, sizeof(blinkapikey));
    strlcpy(blinkwalletid, firstCfg.blinkWalletId, sizeof(blinkwalletid));
    strlcpy(baseURLATM1, firstCfg.baseUrl, sizeof(baseURLATM1));
    strlcpy(secretATM1, firstCfg.secret, sizeof(secretATM1));
    strlcpy(deviceState.currencyATM, firstCfg.currencyATM,
            sizeof(deviceState.currencyATM));
    strlcpy(adminkey, firstCfg.adminKey, sizeof(adminkey));
    strlcpy(readkey, firstCfg.readKey, sizeof(readkey));
    strlcpy(currencyOne, firstCfg.currencyLabel, sizeof(currencyOne));
    billAmountIntOne = firstCfg.billMech;
    maxamount = firstCfg.maxAmount;
    charge1 = firstCfg.charge;
  }

  SecondConfig secondCfg;
  if (configService.loadSecond(FlashFS, SECOND_FILE, secondCfg)) {
    strlcpy(currencyTwo, secondCfg.currencyLabel, sizeof(currencyTwo));
    strlcpy(baseURLATM2, secondCfg.baseUrl, sizeof(baseURLATM2));
    strlcpy(secretATM2, secondCfg.secret, sizeof(secretATM2));
    strlcpy(deviceState.currencyATM2, secondCfg.currencyATM,
            sizeof(deviceState.currencyATM2));
    billAmountIntTwo = secondCfg.billMech;
    maxamount2 = secondCfg.maxAmount;
    charge2 = secondCfg.charge;
  }

  ThirdConfig thirdCfg;
  if (configService.loadThird(FlashFS, THIRD_FILE, thirdCfg)) {
    strlcpy(currencyThree, thirdCfg.currencyLabel, sizeof(currencyThree));
    strlcpy(baseURLATM3, thirdCfg.baseUrl, sizeof(baseURLATM3));
    strlcpy(secretATM3, thirdCfg.secret, sizeof(secretATM3));
    strlcpy(deviceState.currencyATM3, thirdCfg.currencyATM,
            sizeof(deviceState.currencyATM3));
    billAmountIntThree = thirdCfg.billMech;
    maxamount3 = thirdCfg.maxAmount;
    charge3 = thirdCfg.charge;
  }

  if (configService.loadGuiConfig(FlashFS, GUI_FILE, guiConfig)) {
    if (guiConfig.fundingSource[0] != '\0') {
      strlcpy(deviceState.fundingSourceBuffer, guiConfig.fundingSource,
              sizeof(deviceState.fundingSourceBuffer));
      fundingsource = deviceState.fundingSourceBuffer;
    }

    if (guiConfig.rateSource[0] != '\0') {
      strlcpy(deviceState.rateSourceBuffer, guiConfig.rateSource,
              sizeof(deviceState.rateSourceBuffer));
      ratesource = deviceState.rateSourceBuffer;
    }

    if (guiConfig.animated[0] != '\0') {
      strlcpy(deviceState.enableAnimBuffer, guiConfig.animated,
              sizeof(deviceState.enableAnimBuffer));
      animated = deviceState.enableAnimBuffer;
    }

    if (guiConfig.blinkWallet[0] != '\0') {
      strlcpy(deviceState.blinkWalletBuffer, guiConfig.blinkWallet,
              sizeof(deviceState.blinkWalletBuffer));
    }
  }

  int thirdSlash = 0;
  int count = 0;
  for (int i = 0; i < strlen(baseURLATM1); i++) {
    if (baseURLATM1[i] == '/') {
      count++;
      if (count == 3) {
        thirdSlash = i;
        break;
      }
    }
  }
  if (thirdSlash > 0 && thirdSlash < sizeof(lnbitsURL)) {
    strncpy(lnbitsURL, baseURLATM1, thirdSlash);
    lnbitsURL[thirdSlash] = '\0';
  } else {
    strlcpy(lnbitsURL, baseURLATM1, sizeof(lnbitsURL));
  }

  applyPortalCredentials();
  portal.config(acConfig);

  if (appStartupCompleted) {
    // Settings that change what the ATM can pay (funding source, Blink
    // wallet, price source, keys) must be validated before the main screen
    // comes back - landing there with the old wallet's balance is confusing.
    // Re-run the startup check so the new balance/price are confirmed first.
    uiController.deleteMainScreen();
    preflightGate();
    finishStartupAfterPreflight();
  }

  pendingConfigReload = false;
}


/**
 * @brief The String class provides a way to manipulate and store strings of
 * text in Arduino.
 *
 * The String class enables you to work with strings of text in Arduino
 * sketches. It provides various methods for manipulating and accessing string
 * data. The String class is based on the C++ `String` class and provides
 * similar functionality.
 */
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  const int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void to_upper(char *arr) {
  for (size_t i = 0; i < strlen(arr); i++) {
    if (arr[i] >= 'a' && arr[i] <= 'z') {
      arr[i] = arr[i] - 'a' + 'A';
    }
  }
}

// Reads exactly len body bytes from an open response. Gives up after 20 s
// without progress: a server that stalls without closing would otherwise hang
// loop() forever. progressLabel, when set, shows the percentage every 32 KB.
static size_t readResponseBytes(HTTPClient &response, uint8_t *dst, size_t len,
                                lv_obj_t *progressLabel) {
  WiFiClient *stream = response.getStreamPtr();
  if (stream == nullptr) {
    return 0;
  }
  size_t got = 0;
  unsigned long lastDataAt = millis();
  while (got < len && millis() - lastDataAt <= 20000) {
    const int avail = stream->available();
    if (avail <= 0) {
      if (!response.connected()) {
        break;
      }
      delay(1);
      continue;
    }
    const size_t want = len - got;
    const int n = stream->readBytes(
        dst + got, (size_t)avail < want ? (size_t)avail : want);
    if (n <= 0) {
      delay(1);
      continue;
    }
    got += (size_t)n;
    lastDataAt = millis();
    if (progressLabel != nullptr && got % 32768 < (size_t)n) {
      char buf[32];
      snprintf(buf, sizeof(buf), "Downloading... %u%%",
               (unsigned)(100ULL * got / len));
      lv_label_set_text(progressLabel, buf);
      lv_task_handler();
    }
  }
  return got;
}

// State shared between OTA upload handler and done handler.
// Browser uploads .bin via multipart POST — no internet on device required.
static bool otaUploadAborted = false;
static lv_obj_t *otaUploadOverlay = nullptr;

void setup() {
  Serial.begin(115200);
  delay(50);
  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("Reset reason: %s (%d)\n", resetReasonToString(resetReason),
                static_cast<int>(resetReason));
  Serial.println("Booting FIAT HELL on ESP32-8048S050...");
  Serial.println(kFirmwareVersionMarker);
  bootStage(1, "serial ready");

  bootStage(2, "allocating runtime objects");
  deviceStatePtr = new DeviceState();
  sessionStatePtr = new SessionState();
  contentPtr = new String();
  serverPtr = new WebServerClass();
  portalPtr = new AutoConnect(server);
  secureClientPtr = new BundleTlsClient();
  httpPtr = new HTTPClient();
  serialPort1Ptr = new HardwareSerial(1);
  serialPort2Ptr = new HardwareSerial(2);
  BTNAPtr = new Button(BTN1);
  configPtr = new AutoConnectConfig();
  elementsAuxPtr = new AutoConnectAux();
  saveAuxPtr = new AutoConnectAux();
  firstPtr = new AutoConnectConfig();
  firstAuxPtr = new AutoConnectAux();
  savefirstAuxPtr = new AutoConnectAux();
  secondPtr = new AutoConnectConfig();
  secondAuxPtr = new AutoConnectAux();
  savesecondAuxPtr = new AutoConnectAux();
  thirdPtr = new AutoConnectConfig();
  thirdAuxPtr = new AutoConnectAux();
  savethirdAuxPtr = new AutoConnectAux();
  guiPtr = new AutoConnectConfig();
  guiAuxPtr = new AutoConnectAux();
  saveguiAuxPtr = new AutoConnectAux();
  otaAuxPtr = new AutoConnectAux();
  otaDoAuxPtr = new AutoConnectAux();
  uiControllerPtr = new UiController(screen_logo, screen_portal, screen_api,
                                     screen_thx, screen_main,
                                     screen_insert_money, screen_qr);
  if ((deviceStatePtr == nullptr) || (sessionStatePtr == nullptr) ||
      (contentPtr == nullptr) || (serverPtr == nullptr) ||
      (portalPtr == nullptr) ||
      (secureClientPtr == nullptr) || (httpPtr == nullptr) ||
      (serialPort1Ptr == nullptr) || (serialPort2Ptr == nullptr) ||
      (BTNAPtr == nullptr)) {
    Serial.println("Failed to allocate runtime objects");
    while (true) {
      delay(1000);
    }
  }
  bootStage(3, "runtime objects allocated");

  fundingsource = deviceState.fundingSourceBuffer;
  ratesource = deviceState.rateSourceBuffer;
  animated = deviceState.enableAnimBuffer;
  content = "<h1>ATM Access-point</br>For easy variable setting</h1>";
  bootStage(4, "runtime state initialized");

  /*********************/
  /*** Init display ***/
  /*********************/
  bootStage(5, "before lcd.init");
  lcd.init();
  bootStage(6, "after lcd.init");
  Serial.println("Display test: RED");
  lcd.fillScreen(0xF800);
  delay(400);
  Serial.println("Display test: GREEN");
  lcd.fillScreen(0x07E0);
  delay(400);
  Serial.println("Display test: BLUE");
  lcd.fillScreen(0x001F);
  delay(400);
  Serial.println("Display test: WHITE");
  lcd.fillScreen(0xFFFF);
  delay(400);
  Serial.println("Display test: BLACK");
  lcd.fillScreen(0x0000);
  delay(150);
  lv_init();  // Initialize lvgl
  bootStage(7, "after lv_init");

  // Set orientation (landscape)
  if (lcd.width() < lcd.height()) {
    lcd.setRotation(lcd.getRotation() ^ 1);
  }
  bootStage(8, "display rotation checked");

  // LVGL buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
  bootStage(9, "lvgl draw buffer ready");

  // Set LVGL display
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  bootStage(10, "lvgl display driver registered");

  // Set LVGL input (touch)
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);
  bootStage(11, "lvgl touch driver registered");

  // Logo / Splash screen
  createLogoScreen();
  lv_task_handler(); // refresh obrazovky
  bootStage(12, "logo screen created");

  /*********************************/
  /*** Initialize periferies  ***/
  /*********************************/
  BTNA.begin(); // BOOT button as fallback input
  delay(10);
  bootStage(13, "button initialized");

  billAcceptorBegin(); // Bill acceptor – leave running; turned off in createMainScreen()
  // The NV10 keeps its enabled state across an ESP32 reset. Inhibit it until
  // armAcceptorIfQuoted() has a usable price, so a bill inserted during boot
  // cannot be taken before any quote can succeed.
  billAcceptorWrite(185);
  if (TX2 >= 0) {
    SerialPort2.begin(4800, SERIAL_8N1, -1, TX2); // Coin mech
  }
  if (INHIBITMECH >= 0) {
    pinMode(INHIBITMECH, OUTPUT);
  }
  bootStage(14, "serial peripherals initialized");

  initCertificateBundle();
  bootStage(15, "certificate bundle loaded");

  // Start logo wait state (non-blocking)
  currentUiState = UI_LOGO_WAIT;
  stateEnterTime = millis();
  bootStage(16, "entering logo wait");

  // Non-blocking wait for tap during logo screen
  // Keep checking for tap while loading config
  while (currentUiState == UI_LOGO_WAIT) {
    lv_task_handler();
    handleUiStateMachine();
    yield(); // Allow other tasks to run
  }
  bootStage(17, "logo wait finished");
  setBootStatus("Loading settings...");

  /******************************************/
  /*** Read params from SPIFFS  ***/
  /******************************************/
  bootStage(18, "before filesystem init");
  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
  bootStage(19, "filesystem initialized");
  if (format == true) {
    SPIFFS.format();
  }
  if (SPIFFS.exists("/payout-error.txt")) {
    File errFile = SPIFFS.open("/payout-error.txt", "r");
    if (errFile) {
      Serial.print("Last payout error: ");
      Serial.println(errFile.readString());
      errFile.close();
    }
  }
  // The self-check's SETUP button asks for the portal via this flag because
  // the logo tap window has already passed by the time the check fails.
  if (FlashFS.exists("/force-portal")) {
    FlashFS.remove("/force-portal");
    triggerAp = true;
    Serial.println("Self-check requested setup => starting config portal");
  }

  // Serial provisioning window for the web flasher (config upload + WiFi
  // scan) - see services/SerialConfigService.h for the protocol.
  SerialConfigService::runSerialConfigWindow(SPIFFS);

  // get the saved details and store in global variables
  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile) {
    DynamicJsonDocument conf(2400);
    DeserializationError error = deserializeJson(conf, paramFile);
    if (error) {
      Serial.print("Failed to parse ");
      Serial.print(PARAM_FILE);
      Serial.print(": ");
      Serial.println(error.c_str());
      triggerAp = true;
    } else {
      const char *conf0Char = conf[0]["value"] | "changeme";
      const char *conf1Char = conf[1]["value"] | "";
      const char *conf2Char = conf[2]["value"] | "";
      const char *conf3Char = conf[3]["value"] | "FIAT HELL";

      strlcpy(deviceState.password, conf0Char, sizeof(deviceState.password));
      strlcpy(atmdesc, conf1Char, sizeof(atmdesc));
      strlcpy(atmsubtitle, conf2Char, sizeof(atmsubtitle));
      strlcpy(atmtitle, conf3Char, sizeof(atmtitle));
    }
  } else {
    Serial.println("Missing /elements.json, using defaults");
    triggerAp = true;
  }
  if (paramFile) {
    paramFile.close();
  }
  bootStage(20, "main params loaded");

  // Save WiFi credentials from /wifi.json into AutoConnect's NVS credential store.
  // AutoConnect reads from there during portal.begin(), so this is the correct hook.
  // The file is written by the web-flasher via USB serial (WRITE_CONFIG:/wifi.json).
  {
    File wifiFile = SPIFFS.open("/wifi.json", "r");
    if (wifiFile) {
      DynamicJsonDocument wifiDoc(256);
      if (deserializeJson(wifiDoc, wifiFile) == DeserializationError::Ok) {
        const char* ssid = wifiDoc["ssid"] | "";
        const char* pwd  = wifiDoc["password"] | "";
        if (ssid[0] != '\0') {
          AutoConnectCredential cred;
          station_config_t stConfig;
          memset(&stConfig, 0, sizeof(stConfig));
          strlcpy((char*)stConfig.ssid,     ssid, sizeof(stConfig.ssid));
          strlcpy((char*)stConfig.password, pwd,  sizeof(stConfig.password));
          memset(stConfig.bssid, 0, sizeof(stConfig.bssid));
          stConfig.dhcp = STA_DHCP;
          cred.save(&stConfig);
          Serial.print("WiFi credentials saved to AutoConnect NVS: ");
          Serial.println(ssid);
          SPIFFS.remove("/wifi.json"); // consumed — don't re-apply on every boot
        }
      }
      wifiFile.close();
    }
  }
  bootStage(21, "wifi.json applied");

  // Config, OTA and /flashkey answer 403 to anyone not on the setup hotspot.
  // Registered before every other route: WebServer uses the first handler
  // that claims a request, and it owns (deletes) registered handlers.
  server.addHandler(new ApOnlyConfigHandler(server));

  server.on("/", []() {
    const bool routeToConfigPortal =
        pendingPortalCompletion || portalRequestedByUser ||
        portalRequiredForMissingConfig || configPortalUp();
    if (routeToConfigPortal) {
      if (!clientOnSoftAp(server.client())) {
        server.send(403, "text/plain",
          "Portal accessible only via AP — hold BOOT button 3 s to enable");
        server.client().stop();
        return;
      }
      server.sendHeader("Location", "/_ac", true);
      server.send(302, "text/plain", "");
      server.client().stop();
      return;
    }

    const String page = content + AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", page);
  });
  bootStage(22, "root route registered");

  // Captive-portal probe URLs (generate_204, hotspot-detect.html, ...) are
  // intentionally NOT handled here: AutoConnect's own onNotFound/_captivePortal
  // catches them and replies with an absolute-URL 302 to the portal, which is
  // what makes phones auto-open the sign-in page (matching WT32). Registering
  // explicit handlers here would shadow that and only offer a relative
  // redirect that Android won't auto-open.
  server.on("/favicon.ico", []() { server.send(204, "text/plain", ""); });

  // On-device Flash API key wizard (see pageflashkey.h). It creates and
  // reveals a spending API key: hotspot-only via ApOnlyConfigHandler, plus the
  // portal's Basic credentials.
  server.on("/flashkey", HTTP_GET, []() {
    if (!portalBasicAuth(server, acConfig.password.c_str())) {
      return;
    }
    server.send(200, "text/html", flashKeyPageHtml(wifiStatus()));
  });
  server.on("/flashkey/run", HTTP_POST, []() {
    if (!portalBasicAuth(server, acConfig.password.c_str())) {
      return;
    }
    server.send(200, "text/html",
                flashKeyRunAndRender(deviceState, configService, FlashFS,
                                     FIRST_FILE, server.arg("phone"),
                                     server.arg("code")));
  });


  elementsAux.load(FPSTR(PAGE_ELEMENTS));
  elementsAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    File param = FlashFS.open(PARAM_FILE, "r");
    if (param) {
      aux.loadElement(param,
                      {"password", "atmdesc", "atmsubtitle", "atmtitle"});
      param.close();
    }

    if (portal.where() == "/config") {
      File param = FlashFS.open(PARAM_FILE, "r");
      if (param) {
        aux.loadElement(param,
                        {"password", "atmdesc", "atmsubtitle", "atmtitle"});
        param.close();
      }
    }
    return String();
  });
  bootStage(22, "elements aux configured");

  // First page start
  //  get the saved details and store in global variables
  FirstConfig firstCfg;
  if (configService.loadFirst(FlashFS, FIRST_FILE, firstCfg)) {
    strlcpy(blinkapikey, firstCfg.blinkApiKey, sizeof(blinkapikey));
    strlcpy(blinkwalletid, firstCfg.blinkWalletId, sizeof(blinkwalletid));
    strlcpy(baseURLATM1, firstCfg.baseUrl, sizeof(baseURLATM1));
    strlcpy(secretATM1, firstCfg.secret, sizeof(secretATM1));
    strlcpy(deviceState.currencyATM, firstCfg.currencyATM,
            sizeof(deviceState.currencyATM));
    strlcpy(adminkey, firstCfg.adminKey, sizeof(adminkey));
    strlcpy(readkey, firstCfg.readKey, sizeof(readkey));
    strlcpy(currencyOne, firstCfg.currencyLabel, sizeof(currencyOne));
    billAmountIntOne = firstCfg.billMech;
    maxamount = firstCfg.maxAmount;
    charge1 = firstCfg.charge;
    //} else {
    // triggerAp = true;
  }
  bootStage(23, "first config loaded");

  firstAux.load(FPSTR(PAGE_FIRST));
  firstAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    configService.loadAuxConfig(FlashFS, FIRST_FILE, aux,
                                {"blinkapikey", "blinkwalletid", "lnurl",
                                 "adminkey", "readkey", "currencyOne",
                                 "billmech", "maxamount", "charge1"});

    if (portal.where() == "/first") {
      configService.loadAuxConfig(FlashFS, FIRST_FILE, aux,
                                  {"blinkapikey", "blinkwalletid", "lnurl",
                                   "adminkey", "readkey", "currencyOne",
                                   "billmech", "maxamount", "charge1"});
    }
    return String();
  });
  bootStage(24, "first aux configured");

  // Second page start
  // get the saved details and store in global variables
  SecondConfig secondCfg;
  if (configService.loadSecond(FlashFS, SECOND_FILE, secondCfg)) {
    strlcpy(currencyTwo, secondCfg.currencyLabel, sizeof(currencyTwo));
    strlcpy(baseURLATM2, secondCfg.baseUrl, sizeof(baseURLATM2));
    strlcpy(secretATM2, secondCfg.secret, sizeof(secretATM2));
    strlcpy(deviceState.currencyATM2, secondCfg.currencyATM,
            sizeof(deviceState.currencyATM2));
    billAmountIntTwo = secondCfg.billMech;
    maxamount2 = secondCfg.maxAmount;
    charge2 = secondCfg.charge;
    //} else {
    // triggerAp = true;
  }
  bootStage(25, "second config loaded");

  secondAux.load(FPSTR(PAGE_SECOND));
  secondAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    configService.loadAuxConfig(
        FlashFS, SECOND_FILE, aux,
        {"currencyTwo", "lnurl2", "billmech2", "maxamount2", "charge2"});
    if (portal.where() == "/second") {
      configService.loadAuxConfig(
          FlashFS, SECOND_FILE, aux,
          {"currencyTwo", "lnurl2", "billmech2", "maxamount2", "charge2"});
    }
    return String();
  });
  bootStage(26, "second aux configured");

  //*
  //*
  //*
  // get the saved details and store in global variables
  ThirdConfig thirdCfg;
  if (configService.loadThird(FlashFS, THIRD_FILE, thirdCfg)) {
    strlcpy(currencyThree, thirdCfg.currencyLabel, sizeof(currencyThree));
    strlcpy(baseURLATM3, thirdCfg.baseUrl, sizeof(baseURLATM3));
    strlcpy(secretATM3, thirdCfg.secret, sizeof(secretATM3));
    strlcpy(deviceState.currencyATM3, thirdCfg.currencyATM,
            sizeof(deviceState.currencyATM3));
    billAmountIntThree = thirdCfg.billMech;
    maxamount3 = thirdCfg.maxAmount;
    charge3 = thirdCfg.charge;
    //} else {
    // triggerAp = true;
  }
  bootStage(27, "third config loaded");

  thirdAux.load(FPSTR(PAGE_THIRD));
  thirdAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    configService.loadAuxConfig(
        FlashFS, THIRD_FILE, aux,
        {"currencyThree", "lnurl3", "billmech3", "maxamount3", "charge3"});
    if (portal.where() == "/third") {
      configService.loadAuxConfig(
          FlashFS, THIRD_FILE, aux,
          {"currencyThree", "lnurl3", "billmech3", "maxamount3", "charge3"});
    }
    return String();
  });
  bootStage(28, "third aux configured");

  bootStage(29, "before second filesystem init");
  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
  bootStage(30, "after second filesystem init");
  if (format == true) {
    SPIFFS.format();
  }

  // Gui page start - use ConfigService to load persisted GUI settings
  if (configService.loadGuiConfig(FlashFS, GUI_FILE, guiConfig)) {
    if (guiConfig.fundingSource[0] != '\0') {
      strlcpy(deviceState.fundingSourceBuffer, guiConfig.fundingSource,
              sizeof(deviceState.fundingSourceBuffer));
      fundingsource = deviceState.fundingSourceBuffer;
      Serial.print("fundingsource: ");
      Serial.println(deviceState.fundingSourceBuffer);
    }

    if (guiConfig.rateSource[0] != '\0') {
      strlcpy(deviceState.rateSourceBuffer, guiConfig.rateSource,
              sizeof(deviceState.rateSourceBuffer));
      ratesource = deviceState.rateSourceBuffer;
      Serial.print("ratesource: ");
      Serial.println(ratesource);
    }

    if (guiConfig.animated[0] != '\0') {
      strlcpy(deviceState.enableAnimBuffer, guiConfig.animated,
              sizeof(deviceState.enableAnimBuffer));
      animated = deviceState.enableAnimBuffer;
      Serial.print("animated: ");
      Serial.println(animated);
    }

    if (guiConfig.blinkWallet[0] != '\0') {
      strlcpy(deviceState.blinkWalletBuffer, guiConfig.blinkWallet,
              sizeof(deviceState.blinkWalletBuffer));
      Serial.print("blinkwallet: ");
      Serial.println(deviceState.blinkWalletBuffer);
    }
    //} else {
    // triggerAp = true;
  }
  bootStage(31, "gui config loaded");

  // The saved /gui.json carries the radio option lists from the firmware
  // that wrote it, and loadElement() restores them over the page definition -
  // a file written by an older build hides options added since (e.g. Yadio).
  // Rebuild the options from the firmware list and keep the selection.
  static const char *const kFundingSources[] = {"Blink", "LNbits", "Flash"};
  static const char *const kRateSources[] = {"CoinGecko", "ExchangeApi",
                                             "CoinYEP", "Kraken", "Yadio"};
  static const char *const kAnimatedOptions[] = {"No", "Yes"};
  static const char *const kBlinkWallets[] = {"Bitcoin", "Stablesats"};
  auto ensureRadioOptions = [](AutoConnectAux &aux, const char *name,
                               const char *const *options, size_t count) {
    AutoConnectElement *elm = aux.getElement(name);
    if (!elm || elm->typeOf() != AC_Radio) {
      return;
    }
    AutoConnectRadio *radio = static_cast<AutoConnectRadio *>(elm);
    bool same = radio->size() == count;
    for (size_t i = 0; same && i < count; i++) {
      same = radio->at(i).equals(options[i]);
    }
    if (same) {
      return;
    }
    String selected;
    if (radio->checked >= 1 && radio->checked <= radio->size()) {
      selected = radio->at(radio->checked - 1);
    }
    radio->empty(count);
    for (size_t i = 0; i < count; i++) {
      radio->add(options[i]);
    }
    radio->checked = 1;
    if (selected.length() > 0) {
      radio->check(selected);
    }
  };

  guiAux.load(FPSTR(PAGE_GUI));
  guiAux.on([ensureRadioOptions](AutoConnectAux &aux, PageArgument &arg) {
    File paramGui = FlashFS.open(GUI_FILE, "r");
    if (paramGui) {
      aux.loadElement(paramGui,
                      {"fundingsource", "ratesource", "animated", "blinkwallet"});
      paramGui.close();
    }

    if (portal.where() == "/gui") {
      File paramGui = FlashFS.open(GUI_FILE, "r");
      if (paramGui) {
        aux.loadElement(paramGui, {"fundingsource", "ratesource", "animated",
                                   "blinkwallet"});
        paramGui.close();
      }
    }
    ensureRadioOptions(aux, "fundingsource", kFundingSources,
                       sizeof(kFundingSources) / sizeof(kFundingSources[0]));
    ensureRadioOptions(aux, "ratesource", kRateSources,
                       sizeof(kRateSources) / sizeof(kRateSources[0]));
    ensureRadioOptions(aux, "animated", kAnimatedOptions,
                       sizeof(kAnimatedOptions) / sizeof(kAnimatedOptions[0]));
    ensureRadioOptions(aux, "blinkwallet", kBlinkWallets,
                       sizeof(kBlinkWallets) / sizeof(kBlinkWallets[0]));
    return String();
  });
  bootStage(32, "gui aux configured");

  //*
  //*
  //*
  // Save page one
  saveAux.load(FPSTR(PAGE_SAVE));
  saveAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    const AutoConnectElement *passwordEl = elementsAux.getElement("password");
    if (passwordEl == nullptr ||
        !portalPasswordAccepted(passwordEl->value.c_str())) {
      aux["echo"].value =
          "Password must be 8-63 characters and cannot be changeme.";
      return String();
    }
    aux["caption"].value = PARAM_FILE;
    File param = FlashFS.open(PARAM_FILE, "w");
    if (param) {
      // save as a loadable set for parameters.
      elementsAux.saveElement(
          param, {"password", "atmdesc", "atmsubtitle", "atmtitle"});
      param.close();
      // read the saved elements again to display.
      param = FlashFS.open(PARAM_FILE, "r");
      aux["echo"].value = param.readString();
      param.close();
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    pendingConfigReload = true;
    return String();
  });
  bootStage(33, "save aux configured");

  // Save first page
  savefirstAux.load(FPSTR(FIRST_SAVE));
  savefirstAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = FIRST_FILE;
    String echo;
    if (configService.saveAuxConfig(FlashFS, FIRST_FILE, firstAux,
                                    {"blinkapikey", "blinkwalletid", "lnurl",
                                     "adminkey", "readkey", "currencyOne",
                                     "billmech", "maxamount", "charge1"},
                                    echo)) {
      aux["echo"].value = echo;
      pendingConfigReload = true;
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });
  bootStage(34, "save first aux configured");

  // Save second page
  savesecondAux.load(FPSTR(SECOND_SAVE));
  savesecondAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = SECOND_FILE;
    String echo;
    if (configService.saveAuxConfig(
            FlashFS, SECOND_FILE, secondAux,
            {"currencyTwo", "lnurl2", "billmech2", "maxamount2", "charge2"},
            echo)) {
      aux["echo"].value = echo;
      pendingConfigReload = true;
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });
  bootStage(35, "save second aux configured");

  // Save third page
  savethirdAux.load(FPSTR(THIRD_SAVE));
  savethirdAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = THIRD_FILE;
    String echo;
    if (configService.saveAuxConfig(
            FlashFS, THIRD_FILE, thirdAux,
            {"currencyThree", "lnurl3", "billmech3", "maxamount3", "charge3"},
            echo)) {
      aux["echo"].value = echo;
      pendingConfigReload = true;
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });
  bootStage(36, "save third aux configured");

  // Save gui page
  saveguiAux.load(FPSTR(GUI_SAVE));
  saveguiAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = GUI_FILE;
    String echo;
    if (configService.saveAuxConfig(FlashFS, GUI_FILE, guiAux,
                                    {"fundingsource", "ratesource", "animated",
                                     "blinkwallet"},
                                    echo)) {
      aux["echo"].value = echo;
      pendingConfigReload = true;
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });
  bootStage(37, "save gui aux configured");

  originalSizeOne = billAmountIntOne.size();
  originalSizeTwo = billAmountIntTwo.size();
  originalSizeThree = billAmountIntThree.size();
  bootStage(38, "bill vectors sized");

  // First merge billAmountIntOne and billAmountIntTwo
  if ((deviceState.currencyATM2[0] != '\0') || (currencyTwo[0] != '\0')) {
    billAmountIntOne.insert(billAmountIntOne.end(), billAmountIntTwo.begin(),
                            billAmountIntTwo.end());
  } else {
    // Channel ranges and per-currency crediting count these slots; a
    // currency that was not merged has none.
    originalSizeTwo = 0;
  }
  // Check if currencyATM3 is not empty
  if ((deviceState.currencyATM3[0] != '\0') || (currencyThree[0] != '\0')) {
    // Then merge billAmountIntThree into the now-extended billAmountIntOne
    billAmountIntOne.insert(billAmountIntOne.end(), billAmountIntThree.begin(),
                            billAmountIntThree.end());
  } else {
    originalSizeThree = 0;
  }
  bootStage(39, "bill vectors merged");

  /*********************************************************/
  /*** Set AutoConnect before launching the portal       ***/
  /*********************************************************/
  acConfig.auth = AC_AUTH_BASIC;
  acConfig.authScope = AC_AUTHSCOPE_PORTAL;
  // No status LED on this board: the ticker's default pin (LED_BUILTIN) maps
  // to GPIO48 via RMT, which is the RGB panel's DATA_R1 line.
  acConfig.ticker = false;
  acConfig.autoReset = false;
  acConfig.autoReconnect = true;
  acConfig.retainPortal = true;
  acConfig.autoRise = false; // set dynamically during startup based on mode
  acConfig.apid = "LN ATM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  // A stored password that fails the policy must not leave the hotspot open
  // or set to the known default. While it is invalid the hotspot and the web
  // login use a random per-boot secret shown on the portal screen, so only
  // someone standing at the machine can join.
  const bool passwordRejected = !portalPasswordAccepted(deviceState.password);
  applyPortalCredentials();
  // AutoConnect 1.4.2 defaults apip to 172.217.28.1 (Google IP, surprising).
  // Force the standard 192.168.4.0/24 so the apClient subnet check works
  // and so users see a familiar AP IP in the browser URL bar.
  acConfig.apip    = IPAddress(192, 168, 4, 1);
  acConfig.gateway = IPAddress(192, 168, 4, 1);
  acConfig.netmask = IPAddress(255, 255, 255, 0);
  acConfig.menuItems =
      AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS |
      AC_MENUITEM_DEVINFO | AC_MENUITEM_RESET | AC_MENUITEM_HOME;
  acConfig.title = "LN ATM";
  acConfig.homeUri = "/_ac";
  acConfig.reconnectInterval = 1;
  acConfig.channel = 6;        // Fixed channel for stable AP (avoids scan disrupting clients)
  acConfig.beginTimeout = 12000; // 12 s — fast fallback to AP if saved WiFi unreachable
  acConfig.immediateStart =
      false; // If we don't have WiFi saved, it will start AP
  acConfig.username = "admin";
  bootStage(40, "autoconnect config prepared");

  // Register all Aux pages to the portal
  otaAux.load(FPSTR(PAGE_OTA));
  otaAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    // Populate version select from catalog
    HttpsSession catalog;
    HTTPClient &catalogHttp = catalog.httpClient;
    catalogHttp.setTimeout(8000);
    if (catalog.begin(OTA_CATALOG_URL)) {
      int code = catalogHttp.GET();
      if (code == 200) {
        String payload = catalogHttp.getString();
        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, payload) == DeserializationError::Ok &&
            doc.is<JsonArray>()) {
          AutoConnectSelect &versionSelect =
              aux["version"].as<AutoConnectSelect>();
          versionSelect.empty(16);
          for (JsonObject item : doc.as<JsonArray>()) {
            const char *name = item["name"];
            if (name && item["type"] == "bin" &&
                !firmwareUpdateRejected(name, FW_VERSION)) {
              const char *date = item["date"] | "";
              size_t sizeVal = item["size"] | 0;
              String label = String(name);
              if (date[0])
                label += " (" + String(date);
              if (sizeVal > 0)
                label += date[0] ? ", " : " (";
              if (sizeVal > 0)
                label += String(sizeVal / 1024) + " KB";
              if (date[0] || sizeVal > 0)
                label += ")";
              versionSelect.add(label);
            }
          }
          if (versionSelect.size() == 0)
            versionSelect.add("No firmware found");
        } else {
          aux["version"].as<AutoConnectSelect>().empty(1);
          aux["version"].as<AutoConnectSelect>().add("Catalog parse error");
        }
      } else {
        aux["version"].as<AutoConnectSelect>().empty(1);
        aux["version"].as<AutoConnectSelect>().add("Catalog fetch failed");
      }
      catalogHttp.end();
    } else {
      aux["version"].as<AutoConnectSelect>().empty(1);
      aux["version"].as<AutoConnectSelect>().add("Connection failed");
    }
    return String();
  }, AC_EXIT_AHEAD);
  otaDoAux.load(FPSTR(PAGE_OTA_DO));
  otaDoAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    String selected = arg.arg("version");
    // Extract filename: "fiat-hell-v1.2.0.bin (2026-03-08, 1911 KB)" -> "fiat-hell-v1.2.0.bin"
    int parenIdx = selected.indexOf(" (");
    String filename = parenIdx > 0 ? selected.substring(0, parenIdx) : selected;
    filename.trim();
    if (!filename.endsWith(".bin"))
      filename = "";
    if (filename.length() == 0) {
      aux["result"].value = "No version selected.";
      return String();
    }
    if (firmwareUpdateRejected(filename.c_str(), FW_VERSION)) {
      aux["result"].value = "Refused: missing or older version.";
      return String();
    }
    String updateUrl = String(OTA_BASE_URL) + "/" + filename;
    static lv_obj_t *otaOverlay = nullptr;
    otaOverlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(otaOverlay, screenWidth, screenHeight);
    lv_obj_set_pos(otaOverlay, 0, 0);
    lv_obj_set_style_bg_color(otaOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(otaOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(otaOverlay, 0, 0);
    lv_obj_clear_flag(otaOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *otaLabel = lv_label_create(otaOverlay);
    lv_label_set_text(otaLabel, "Firmware update...\nPlease wait.");
    lv_obj_center(otaLabel);
    lv_obj_set_style_text_font(otaLabel, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(otaLabel, lv_color_white(), 0);
    lv_obj_move_foreground(otaOverlay);
    for (int i = 0; i < 8; i++) {
      lv_task_handler();
      delay(30);
    }
    auto failUpdate = [&](const String &message) {
      lv_obj_del(otaOverlay);
      otaOverlay = nullptr;
      lv_task_handler();
      aux["result"].value = message;
    };

    // The OTA slot receives nothing until the signature over the whole
    // image checks out, so the image is buffered in PSRAM laid out as
    // signature(64) || image — the input crypto_sign_open expects.
    HttpsSession bin;
    bin.httpClient.setTimeout(20000);
    if (!bin.begin(updateUrl) || bin.httpClient.GET() != 200) {
      failUpdate("Update failed: firmware download");
      return String();
    }
    const int imageLen = bin.httpClient.getSize();
    if (imageLen <= 0) {
      failUpdate("Update failed: server sent no content length");
      return String();
    }
    if ((size_t)imageLen > ESP.getFreeSketchSpace()) {
      failUpdate("Update failed: image larger than the OTA slot");
      return String();
    }
    uint8_t *signedImage =
        (uint8_t *)heap_caps_malloc((size_t)imageLen + 64, MALLOC_CAP_SPIRAM);
    if (signedImage == nullptr) {
      failUpdate("Update failed: no memory");
      return String();
    }
    const size_t written = readResponseBytes(
        bin.httpClient, signedImage + 64, (size_t)imageLen,
        lv_obj_get_child(otaOverlay, 0));
    bin.httpClient.end();
    if (written != (size_t)imageLen) {
      free(signedImage);
      failUpdate("Update failed: short download");
      return String();
    }

    HttpsSession sig;
    sig.httpClient.setTimeout(15000);
    const bool sigFetched = sig.begin(updateUrl + ".sig") &&
                            sig.httpClient.GET() == 200 &&
                            sig.httpClient.getSize() == 64 &&
                            readResponseBytes(sig.httpClient, signedImage, 64,
                                              nullptr) == 64;
    sig.httpClient.end();
    if (!sigFetched || !otaSignatureOk(signedImage, written + 64)) {
      free(signedImage);
      failUpdate("Update failed: bad signature");
      return String();
    }
    // Checked after the signature so only authenticated bytes are parsed.
    // The marker is inside the image, so a renamed file cannot fake it.
    if (firmwareImageDowngrade(signedImage + 64, written, FW_VERSION)) {
      free(signedImage);
      failUpdate("Update failed: older firmware");
      return String();
    }
    lv_label_set_text(lv_obj_get_child(otaOverlay, 0), "Installing...");
    lv_task_handler();
    // Every Update failure path resets itself; errorString() must be read
    // before anything else touches Update.
    const bool installed = Update.begin(written) &&
                           Update.write(signedImage + 64, written) == written &&
                           Update.end(true);
    free(signedImage);
    if (!installed) {
      failUpdate("Update failed: " + String(Update.errorString()));
      return String();
    }
    lv_label_set_text(lv_obj_get_child(otaOverlay, 0), "Update OK. Rebooting...");
    lv_task_handler();
    aux["result"].value = "Update OK. Rebooting...";
    // Defer the restart so the /ota_do response reaches the browser first.
    pendingRestartAt = millis() + 1000;
    return String();
  }, AC_EXIT_AHEAD);
  portal.join({elementsAux, saveAux, firstAux, savefirstAux, secondAux,
               savesecondAux, thirdAux, savethirdAux, guiAux, saveguiAux,
               otaAux, otaDoAux});
  bootStage(41, "portal aux pages joined");

  // Apply config
  portal.config(acConfig);
  bootStage(42, "portal config applied");

  // Create the loading indicator
  createLoadingIndicator();
  lv_task_handler();
  delay(5);
  bootStage(43, "loading indicator created");

  ///*** Debug ***////
  /*Serial.print(F("APP PASSWORD: "));
  Serial.println(password);
  Serial.print(F("Admin key: "));
  Serial.println(adminkey);
  Serial.print(F("Read key: "));
  Serial.println(readkey);
  Serial.print(F("Blink API key: "));
  Serial.println(blinkapikey);
  Serial.print(F("Blink wallet ID: "));
  Serial.println(blinkwalletid);
  Serial.print(F("Funding source: "));
  Serial.println(fundingSourceBuffer);
  Serial.print(F("Switch enabled: "));
  Serial.println(ratesource);
  Serial.print(F("Animation enabled: "));
  Serial.println(animated);
  Serial.print(F("Currency selected: "));
  Serial.println(currencySelected);
  Serial.print(F("Main currency: "));
  Serial.println(currencyOne);
  Serial.print(F("Second currency: "));
  Serial.println(currencyTwo);
  Serial.print(F("Third currency: "));
  Serial.println(currencyThree);
  Serial.print(F("ATM currency: "));
  Serial.println(currencyATM);
  Serial.print(F("ATM2 currency: "));
  Serial.println(currencyATM2);
  Serial.print(F("ATM3 currency: "));
  Serial.println(currencyATM3);
  Serial.print(F("MAX (selected): "));
  Serial.println(maxamountSelected);
  Serial.print(F("Charge: "));
  Serial.println(charge1);*/

  /**************************************************************************/
  /***  Starting AutoConnect - connection attempt or AP (portal)         ***/
  /**************************************************************************/

  const bool isGaloyMode =
      FundingService::isGaloy(deviceState.fundingSourceBuffer);
  const bool wifiRequired = isGaloyMode;
  const bool userWantsPortal = triggerAp; // tap during logo window

  const bool apiDataMissing =
      ((strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0 &&
        (deviceState.currencyATM[0] == '\0' || adminkey[0] == '\0' ||
         readkey[0] == '\0')) ||
       // Both Galoy sources need the API key; only Blink needs a wallet id
       // up front - Flash resolves its wallet via fetchGaloyBalance().
       (isGaloyMode && blinkapikey[0] == '\0') ||
       (paymentService.isBlink(deviceState.fundingSourceBuffer) &&
        blinkwalletid[0] == '\0') ||
       (currencyOne[0] == '\0'));

  portalRequestedByUser = userWantsPortal;
  // A missing key or currency used to open this screen on every boot. The
  // startup check already names what is missing, and SETUP there (or a tap on
  // the logo) is how the operator asks for the hotspot.
  portalRequiredForMissingConfig = passwordRejected;
  portalRequiredForWifiRecovery = (wifiRequired && !wifiStatus());

  const bool showPortalScreenImmediately =
      (userWantsPortal || portalRequiredForMissingConfig);

  // In config-first mode, keep the AP stable for phones instead of trying to
  // reconnect to a remembered WiFi in the background. Background STA scans in
  // AP+STA mode make the SoftAP hop channels, so phones drop the connection
  // ("connects and disconnects") - so we also stop retries when the portal is
  // up because WiFi failed (the recovery case), not just on explicit entry.
  // reconnectInterval=0 is what stops those background scans. An explicit
  // setup hotspot does not join the saved network: AP+STA makes
  // fiathell.local advertise two addresses and the page load waits on the
  // one the phone cannot reach. Normal boot still tries the saved network.
  const bool portalActiveNow = portalRequestedByUser ||
                               portalRequiredForMissingConfig ||
                               portalRequiredForWifiRecovery;
  const bool explicitPortal =
      userWantsPortal || portalRequiredForMissingConfig;
  acConfig.autoReconnect = !explicitPortal;
  acConfig.reconnectInterval = portalActiveNow ? 0 : 1;
  acConfig.preserveAPMode = portalActiveNow;

  // Decide portal behavior once, then call portal.begin() once.
  acConfig.immediateStart =
      (userWantsPortal || portalRequiredForMissingConfig);
  acConfig.autoRise =
      (userWantsPortal || portalRequiredForMissingConfig || wifiRequired);
  // Keep AutoConnect's DNSServer off port 53 (our apDns answers instead) and
  // skip its blocking captive loop; the portal is serviced from loop().
  portal.onDetect([](IPAddress &) { return false; });
  WiFi.onEvent(onWifiDiagEvent);
  // The S3 core defaults to modem sleep, which holds every incoming packet
  // until the next beacon wake-up (0.1-1.3 s ping measured): portal pages and
  // every Blink/LNbits HTTPS round trip crawl. Must be set before STA starts.
  WiFi.setSleep(false);

  if (portalActiveNow) {
    Serial.println("Config portal active: stopping STA retries to keep captive AP stable");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
  }

  if (isGaloyMode) {
    Serial.print(deviceState.fundingSourceBuffer);
    Serial.println(" mode => Internet needed");
  } else if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    Serial.println("LNbits mode => offline possible");
  } else {
    Serial.print("Unrecognized funding source: '");
    Serial.print(deviceState.fundingSourceBuffer);
    Serial.println("'");
  }

  if (userWantsPortal) {
    Serial.println("User tap => start AP portal immediately");
  } else if (passwordRejected) {
    Serial.println("Default password => start AP portal immediately");
  } else {
    Serial.println("No tap => try STA first");
    if (apiDataMissing) {
      Serial.println("Settings incomplete; startup check will report them");
    }
  }
  bootStage(44, "portal mode decision made");

  bool portalScreenShown = false;
  if (showPortalScreenImmediately) {
    createPortalScreen();
    lv_task_handler();
    delay(50);
    bootStage(45, "portal screen shown");
    portalScreenShown = true;
  }

  portal.config(acConfig);
  bootStage(46, "portal config re-applied");
  Serial.println("Attempting to connect via AutoConnect...");
  setBootStatus("Connecting to Wi-Fi...");
  (void)portal.begin(); // may connect STA or start AP depending on config
  bootStage(47, "portal begin returned");

  // NOTE: we intentionally do NOT register our own onNotFound here. AutoConnect
  // installs its _handleNotFound/_captivePortal in begin(); leaving it in place
  // gives WT32-identical captive behaviour (absolute-URL 302 to the portal,
  // host-header based) so phones auto-open the sign-in page.

  if (wifiStatus()) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString() +
                   " RSSI: " + String(WiFi.RSSI()) + " dBm ch " +
                   String(WiFi.channel()));
    const bool stayingInPortal =
        userWantsPortal || portalRequiredForMissingConfig;
    if (wifiRequired && !stayingInPortal) {
      // If you don't want to leave the AP on, switch to STA only
      WiFi.mode(WIFI_STA);
    }
  } else {
    Serial.println("WiFi not connected.");
    if (acConfig.autoRise) {
      if (!portalScreenShown) {
        createPortalScreen();
        lv_task_handler();
        delay(50);
        bootStage(45, "portal screen shown");
        portalScreenShown = true;
      }
      Serial.println("Portal available. AP Name: " + acConfig.apid);
      Serial.println("Portal AP IP: " + WiFi.softAPIP().toString());
      ensureApDns();
      digitalWrite(11, LOW);
    }
  }
  // After begin() the radio may still be AP+STA. Collapse an explicit setup
  // session to the hotspot alone, then publish fiathell.local for that one IP.
  if (explicitPortal) {
    settleSetupHotspot();
  } else if (!wifiStatus() && acConfig.autoRise) {
    startFiathellMdns();
  }
  bootStage(48, "wifi or portal state evaluated");

  // If portal is required (tap / missing data / Blink no-wifi), stay in portal.
  if (userWantsPortal || portalRequiredForMissingConfig ||
      (wifiRequired && !wifiStatus())) {
    pendingPortalCompletion = true;
    bootStage(49, "setup exits into portal mode");
    return;
  }

  completeStartupAfterPortal();
}

/**
 * Reads a single byte from the SerialPort1 if data is available.
 * This function is non-blocking, meaning it returns immediately
 * whether data is available or not.
 *
 * @return The byte read from the SerialPort1, or -1 if no data is available.
 */
int nonBlockingRead() {
  return billAcceptorRead();
}

// More than one currency is configured. Opening every channel of a single
// currency is not mixed mode — that path was treating a PYG note as euros
// and ending the sale at the 100 EUR cap after the first bill.
static bool mixedCurrenciesConfigured() {
  return originalSizeTwo > 0 || originalSizeThree > 0;
}

// Credit a validated channel byte to the running totals. Returns false when
// the channel is outside the single-currency filter. Screen and label
// updates stay with the caller.
static bool creditBillTotals(int channelIdx) {
  const int amount = billAmountIntOne[channelIdx];
  const bool mixed = mixedCurrenciesConfigured();
  if (mixed) {
    if (channelIdx < (int)originalSizeOne) {
      sessionState.totalCurrency1 += (long)amount * 100;
      strlcpy(sessionState.lastBillCurrency, currencyOne,
              sizeof(sessionState.lastBillCurrency));
    } else if (channelIdx < (int)(originalSizeOne + originalSizeTwo)) {
      sessionState.totalCurrency2 += (long)amount * 100;
      strlcpy(sessionState.lastBillCurrency, currencyTwo,
              sizeof(sessionState.lastBillCurrency));
    } else {
      sessionState.totalCurrency3 += (long)amount * 100;
      strlcpy(sessionState.lastBillCurrency, currencyThree,
              sizeof(sessionState.lastBillCurrency));
    }
    sessionState.lastBillCents = (long)amount * 100;
    return true;
  }
  if (channelIdx >= sessionState.allowedChannelStart &&
      channelIdx <
          sessionState.allowedChannelStart + sessionState.allowedChannelCount) {
    bills = bills + amount;
    total = (coins + bills);
    return true;
  }
  return false;
}

// A note already past the NV10's inhibit point is still stacked and reports
// its channel afterwards. Stop the acceptor, then give such a note time to
// report so it is part of the payout instead of being taken uncredited.
static const unsigned long BILL_SETTLE_MS = 2000;

static void inhibitAndCollectPendingBills() {
  billAcceptorWrite(185);
  if (INHIBITMECH >= 0) {
    digitalWrite(INHIBITMECH, LOW);
  }
  const unsigned long start = millis();
  while (millis() - start < BILL_SETTLE_MS) {
    const int pending = nonBlockingRead();
    if (pending >= 1 && pending <= (int)billAmountIntOne.size()) {
      creditBillTotals(pending - 1);
    } else {
      lv_task_handler();
      delay(10);
    }
  }
}

// Create the logo screen
/**
 * @brief Sets the angle of an LVGL arc object.
 * This function is used to set the angle of an LVGL arc object.
 * @param obj Pointer to the LVGL arc object.
 * @param v The angle value to set.
 */
static lv_obj_t *logoArc = nullptr;
static lv_obj_t *logoBtcImg = nullptr;

static void set_angle(void *obj, int32_t v) {
  lv_arc_set_value((lv_obj_t *)obj, v);
}

// Tap on the boot logo asks for settings. Hide the Bitcoin icon and replace
// the one-shot fill with the same endless spinner used for payouts.
static void showLogoSettingsProgress() {
  if (logoBtcImg != nullptr) {
    lv_obj_add_flag(logoBtcImg, LV_OBJ_FLAG_HIDDEN);
  }
  if (logoArc != nullptr) {
    const lv_coord_t w = lv_obj_get_width(logoArc);
    const lv_coord_t h = lv_obj_get_height(logoArc);
    lv_anim_del(logoArc, set_angle);
    lv_obj_del(logoArc);
    logoArc = nullptr;
    lv_obj_t *spinner = lv_spinner_create(screen_logo, 1000, 70);
    if (w > 0 && h > 0) {
      lv_obj_set_size(spinner, w, h);
    }
    lv_obj_center(spinner);
    styleWaitSpinner(spinner);
  }
  lv_refr_now(NULL);
}

/**
 * Checks the status of the WiFi connection.
 * @return true if the WiFi is connected, false otherwise.
 */
bool wifiStatus() { return (WiFi.status() == WL_CONNECTED); }

/**
 * @brief Creates a logo screen with a logo, URL label, arc animation, and an
 * image. This function creates a new screen and adds various graphical elements
 * to it, including a logo, a URL label, an arc animation, and an image. The
 * logo screen is then loaded and displayed.
 * @note The function assumes that the necessary resources (e.g., fonts, images)
 * have been properly initialized and loaded beforehand.
 */
void createLogoScreen() {
  screen_logo = lv_obj_create(NULL); // Create a new screen

  // Put your logo creation code here, but replace `lv_scr_act()` with
  // `screen_logo`
  String LVGL_ATMURL = "ATM.LNPAY.EU";
  lv_obj_t *atmurl =
      lv_label_create(screen_logo); // use screen_logo as the parent
  lv_label_set_text(atmurl, LVGL_ATMURL.c_str());
  lv_obj_align(atmurl, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_text_font(atmurl, &lv_font_montserrat_28,
                             0); // Set font (replace with appropriate font)
  lv_obj_set_style_text_color(atmurl, LV_COLOR_PURPLE, 0);

  /*Create an Arc*/
  logoArc = lv_arc_create(screen_logo); // Create the arc on screen_logo
  lv_arc_set_rotation(logoArc, 270);
  lv_arc_set_bg_angles(logoArc, 0, 360);
  lv_obj_remove_style(logoArc, NULL,
                      LV_PART_KNOB); /*Be sure the knob is not displayed*/
  lv_obj_clear_flag(logoArc,
                    LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/
  styleWaitSpinner(logoArc);
  lv_obj_center(logoArc);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, logoArc);
  lv_anim_set_exec_cb(&a, set_angle);
  lv_anim_set_time(&a, 2000);
  lv_anim_set_repeat_count(&a, 1); /*Just for the demo*/
  lv_anim_set_repeat_delay(&a, 500);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_start(&a);

  logoBtcImg = lv_img_create(screen_logo); // Create an image object
  lv_img_set_src(
      logoBtcImg,
      &btcSmallImg); // Set the image source to your converted image (my_image)
  lv_obj_align(logoBtcImg, LV_ALIGN_CENTER, 0,
               0); // Align the image to the center of the screen

  // The arc animation starts before the screen is shown; load it once.
  lv_scr_load(screen_logo);
  createBatteryIndicator();
  attachBatteryToCurrentScreen();
}

// Create the portal screen
/**
 * @brief Creates the portal screen.
 * This function creates a new screen and adds various labels to display
 * instructions for connecting to a Wi-Fi network.
 * @note The function assumes that the necessary fonts have been loaded and the
 * screen_portal object has been declared globally.
 * @note The labels are aligned vertically and centered horizontally on the
 * screen.
 * @note The text for the labels is set using predefined string constants.
 * @note The font styles for the labels are set using predefined font objects.
 * @note The screen_portal object is loaded as the active screen.
 */
static void portalRestartCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  Serial.println("Portal RESTART tapped");
  pendingRestartAt = millis() + 400;
}

void createPortalScreen() {
  screen_portal = lv_obj_create(NULL); // Create a new screen
  lv_obj_set_style_bg_color(screen_portal, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen_portal, LV_OPA_COVER, 0);

  lv_obj_t *verLabel = lv_label_create(screen_portal);
  lv_label_set_text(verLabel, "v" FW_VERSION);
  lv_obj_align(verLabel, LV_ALIGN_TOP_RIGHT, -15, 10);
  lv_obj_set_style_text_font(verLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(verLabel, lv_color_hex(0x808080), 0);

  String LVGL_PORTAL_ON = "Config launched";
  lv_obj_t *portalon =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portalon, LVGL_PORTAL_ON.c_str()); // set label text
  lv_obj_align(portalon, LV_ALIGN_TOP_MID, 0, 20); // Center but 20 from the top
  lv_obj_set_style_text_font(portalon, &lv_font_montserrat_48,
                             0); // Use the large font
  lv_obj_set_style_text_color(portalon, LV_COLOR_WHITE, 0);

  String LVGL_CONNECT_TO_WIFI = "Connect with your phone via Wi-Fi.";
  lv_obj_t *connecttowifi =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(connecttowifi,
                    LVGL_CONNECT_TO_WIFI.c_str()); // set label text
  lv_obj_align(connecttowifi, LV_ALIGN_TOP_MID, 0,
               80); // Center but 20 from the top
  lv_obj_set_style_text_font(connecttowifi, &lv_font_montserrat_24,
                             0); // Use the large font
  lv_obj_set_style_text_color(connecttowifi, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_ONE = "Find new Wi-Fi network 'LN ATM-xxxx' ";
  lv_obj_t *portaltextone =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextone,
                    LVGL_PORTAL_TEXT_ONE.c_str()); // set label text
  lv_obj_align(portaltextone, LV_ALIGN_TOP_MID, 0,
               120); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextone, &lv_font_montserrat_22,
                             0); // Use the large font
  lv_obj_set_style_text_color(portaltextone, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_TWO =
      "in your phone and connect.";
  lv_obj_t *portaltexttwo =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltexttwo,
                    LVGL_PORTAL_TEXT_TWO.c_str()); // set label text
  lv_obj_align(portaltexttwo, LV_ALIGN_TOP_MID, 0,
               155); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltexttwo, &lv_font_montserrat_22,
                             0); // Slightly smaller for longer text
  lv_obj_set_style_text_color(portaltexttwo, LV_COLOR_WHITE, 0);

  /*String LVGL_PORTAL_TEXT_TWO_B = "\"Use network anyway\". Then open browser:";
  lv_obj_t *portaltext2b = lv_label_create(screen_portal);
  lv_label_set_text(portaltext2b, LVGL_PORTAL_TEXT_TWO_B.c_str());
  lv_obj_align(portaltext2b, LV_ALIGN_TOP_MID, 0, 178);
  lv_obj_set_style_text_font(portaltext2b, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(portaltext2b, LV_COLOR_WHITE, 0);*/

  String LVGL_PORTAL_URL = "http://192.168.4.1   or   http://fiathell.local";
  lv_obj_t *portalurl = lv_label_create(screen_portal);
  lv_label_set_text(portalurl, LVGL_PORTAL_URL.c_str());
  lv_obj_align(portalurl, LV_ALIGN_TOP_MID, 0, 198);
  lv_obj_set_style_text_font(portalurl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(portalurl, lv_color_hex(0x90EE90), 0);

  String LVGL_PORTAL_TEXT_THREE = "After connected, open ATM settings ";
  lv_obj_t *portaltextthree =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextthree,
                    LVGL_PORTAL_TEXT_THREE.c_str()); // set label text
  lv_obj_align(portaltextthree, LV_ALIGN_TOP_MID, 0, 228);
  lv_obj_set_style_text_font(portaltextthree, &lv_font_montserrat_22,
                             0); // Use the large font
  lv_obj_set_style_text_color(portaltextthree, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_FOUR = "and set your preferences";
  lv_obj_t *portaltextfour =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextfour,
                    LVGL_PORTAL_TEXT_FOUR.c_str()); // set label text
  lv_obj_align(portaltextfour, LV_ALIGN_TOP_MID, 0, 262);
  lv_obj_set_style_text_font(portaltextfour, &lv_font_montserrat_22,
                             0); // Use the large font
  lv_obj_set_style_text_color(portaltextfour, LV_COLOR_WHITE, 0);

  if (g_fallbackApPsk.length() > 0) {
    String pskLine = "Wi-Fi and admin password: " + g_fallbackApPsk;
    lv_obj_t *pskLabel = lv_label_create(screen_portal);
    lv_label_set_text(pskLabel, pskLine.c_str());
    lv_obj_align(pskLabel, LV_ALIGN_TOP_MID, 0, 302);
    lv_obj_set_style_text_font(pskLabel, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(pskLabel, lv_color_hex(0xFF9900), 0);
  }

  lv_obj_clear_flag(screen_portal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *restartBtn = lv_btn_create(screen_portal);
  lv_obj_set_size(restartBtn, 240, 64);
  lv_obj_align(restartBtn, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_clear_flag(restartBtn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(restartBtn, portalRestartCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *restartLabel = lv_label_create(restartBtn);
  lv_label_set_text(restartLabel, "RESTART");
  lv_obj_set_style_text_font(restartLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(restartLabel);
  styleActionButton(restartBtn);

  // Labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_portal);
  attachBatteryToCurrentScreen();
}

/**
 * @brief Creates the API screen.
 * This function creates a new screen and adds various labels to display API
 * information. The labels include the API title, restart instructions,
 * connection instructions, and preference instructions.
 * @note The API data is currently set to "API DATA MISSING".
 * @note The labels are aligned and styled using different fonts.
 * @note The screen is loaded after all the labels are created.
 */
void createAPIScreen() {
  screen_api = lv_obj_create(NULL); // Create a new screen

  String LVGL_API = "API DATA MISSING";
  lv_obj_t *apititle = lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(apititle, LVGL_API.c_str());    // set label text
  lv_obj_align(apititle, LV_ALIGN_TOP_MID, 0, 20); // Center but 20 from the top
  lv_obj_set_style_text_font(apititle, &lv_font_montserrat_48,
                             0); // Use the large font
  lv_obj_set_style_text_color(apititle, LV_COLOR_WHITE, 0);

  String LVGL_CONNECT_TO_WIFI = "Connect with your phone via Wi-Fi.";
  lv_obj_t *connecttowifi =
      lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(connecttowifi,
                    LVGL_CONNECT_TO_WIFI.c_str()); // set label text
  lv_obj_align(connecttowifi, LV_ALIGN_TOP_MID, 0,
               80); // Center but 20 from the top
  lv_obj_set_style_text_font(connecttowifi, &lv_font_montserrat_24,
                             0); // Use the large font
  // lv_obj_set_style_text_color(atmurl, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_ONE = "Find new Wi-Fi network 'LN ATM-xxxx' ";
  lv_obj_t *portaltextone =
      lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(portaltextone,
                    LVGL_PORTAL_TEXT_ONE.c_str()); // set label text
  lv_obj_align(portaltextone, LV_ALIGN_TOP_MID, 0,
               120); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextone, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_TWO = "in your phone and connect to it. After ";
  lv_obj_t *portaltexttwo =
      lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(portaltexttwo,
                    LVGL_PORTAL_TEXT_TWO.c_str()); // set label text
  lv_obj_align(portaltexttwo, LV_ALIGN_TOP_MID, 0,
               160); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltexttwo, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_THREE = "you are connected, open ATM settings ";
  lv_obj_t *portaltextthree =
      lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(portaltextthree,
                    LVGL_PORTAL_TEXT_THREE.c_str()); // set label text
  lv_obj_align(portaltextthree, LV_ALIGN_TOP_MID, 0,
               200); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextthree, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_FOUR = "and set your preferences";
  lv_obj_t *portaltextfour =
      lv_label_create(screen_api); // full screen as the parent
  lv_label_set_text(portaltextfour,
                    LVGL_PORTAL_TEXT_FOUR.c_str()); // set label text
  lv_obj_align(portaltextfour, LV_ALIGN_TOP_MID, 0,
               240); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextfour, &lv_font_montserrat_22,
                             0); // Use the large font

  // Labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_api);
  attachBatteryToCurrentScreen();
}

/**
 * Checks the network and device status based on the funding source and other
 * conditions. If the funding source is "Blink" and there is no network
 * connection available, it prints a message and optionally triggers a screen
 * update or indicator. If the funding source is "LNbits" and any of the
 * required data (currencyATM, adminkey, readkey) is missing, it prints a
 * message.
 */
void checkNetworkAndDeviceStatus() {
  if (paymentService.isGaloy(deviceState.fundingSourceBuffer)) {
    if (!wifiStatus()) {
      Serial.println("No network connection available. Checking again soon...");
      // Optionally, trigger a screen update or indicator that network is
      // required but unavailable
      billAcceptorWrite(185);
      if (INHIBITMECH >= 0) {
        digitalWrite(INHIBITMECH, LOW);
      }
    }
  } else if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0 &&
             (deviceState.currencyATM[0] == '\0' || adminkey[0] == '\0' ||
              readkey[0] == '\0')) {
    if (!wifiStatus()) {
      Serial.println("Network not needed, but missing data for LNbits...");
      // SerialPort1.write(184);
    }
  }
}

/**
 * Checks if the funding source is LNbits.
 * @return true if the funding source is LNbits, false otherwise.
 */
bool isLNbits() {
  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    return true;
  } else {
    return false;
  }
  Serial.print("isLNbits: ");
  Serial.println(isLNbits());
}

/**
 * @brief Creates a thank you screen.
 *
 * This function creates a new screen with a thank you message and description.
 * The screen includes a title and a description label, both centered on the
 * screen. The title label uses a large font and green text color. The
 * description label uses a smaller font and green text color.
 * @note The screen_thx global variable must be defined before calling this
 * function.
 */
void createThankYouScreen() {
  screen_thx = lv_obj_create(NULL); // Create a new screen

  String LVGL_THX = "THANK YOU!";
  lv_obj_t *thxTitle = lv_label_create(screen_thx); // full screen as the parent
  lv_label_set_text(thxTitle, LVGL_THX.c_str());    // set label text
  lv_obj_align(thxTitle, LV_ALIGN_CENTER, 0, 0); // Center but 20 from the top
  lv_obj_set_style_text_font(thxTitle, &lv_font_montserrat_48,
                             0); // Use the large font
  lv_obj_set_style_text_color(thxTitle, LV_COLOR_GREEN, 0);

  String LVGL_THX_DESC = "START OVER TO BURN MORE!";
  lv_obj_t *thxDesc = lv_label_create(screen_thx); // full screen as the parent
  lv_label_set_text(thxDesc, LVGL_THX_DESC.c_str()); // set label text
  lv_obj_align(thxDesc, LV_ALIGN_CENTER, 0, 60); // Center but 20 from the top
  lv_obj_set_style_text_font(thxDesc, &lv_font_montserrat_16,
                             0); // Use the large font
  lv_obj_set_style_text_color(thxDesc, LV_COLOR_GREEN, 0);

  // Labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_thx);
  attachBatteryToCurrentScreen();
}

/**
 * @brief Creates a payment error screen.
 *
 * Shown when the funding source rejected the payout after cash was already
 * inserted, so the customer must not walk away thinking they were paid.
 */
// Kept so returnToMainScreen() can delete it; a local pointer leaked one
// screen per failed payout once the error stopped ending in a reboot.
static lv_obj_t *screen_payment_error = nullptr;

void createPaymentErrorScreen() {
  const char *reason = FundingService::payoutFailureReason();
  if (reason == nullptr || reason[0] == '\0') {
    reason = "Blink did not pay the invoice";
  }
  Serial.print("Payment error: ");
  Serial.println(reason);
  File errFile = SPIFFS.open("/payout-error.txt", "w");
  if (errFile) {
    errFile.println(reason);
    errFile.close();
  }

  if (screen_payment_error != nullptr && lv_scr_act() != screen_payment_error) {
    lv_obj_del(screen_payment_error);
  }
  screen_payment_error = lv_obj_create(NULL); // Create a new screen
  lv_obj_t *screen_err = screen_payment_error;

  lv_obj_t *errTitle = lv_label_create(screen_err);
  lv_label_set_text(errTitle, "PAYMENT FAILED!");
  lv_obj_align(errTitle, LV_ALIGN_CENTER, 0, -40);
  lv_obj_set_style_text_font(errTitle, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(errTitle, LV_COLOR_RED, 0);

  char body[240];
  snprintf(body, sizeof(body),
           "YOUR SATS WERE NOT SENT\n%s\nMAKE A PHOTO AND CONTACT SUPPORT",
           reason);
  lv_obj_t *errDesc = lv_label_create(screen_err);
  lv_label_set_text(errDesc, body);
  lv_obj_set_width(errDesc, 740);
  lv_label_set_long_mode(errDesc, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(errDesc, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(errDesc, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_text_font(errDesc, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(errDesc, LV_COLOR_RED, 0);

  // Labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_err);
  attachBatteryToCurrentScreen();
}

/**
 * @brief Updates the burn text label with the combined text of "BURN YOUR
 * {currencySelected} FOR SATS". It also checks the network and device status,
 * price, balance, and updates the main screen label.
 * @note This function assumes that the burnTextLabel has been created.
 * @param None
 * @return None
 */
void updateBurnText() {
  Serial.print("Free heap (updateBurnText Start): ");
  Serial.println(ESP.getFreeHeap());
  if (burnTextLabel) // Ensure the label has been created
  {
    String combinedText = "BURN YOUR SHITCOIN FOR SATS";
    lv_label_set_text(burnTextLabel, combinedText.c_str());

    checkNetworkAndDeviceStatus();
    // checkPrice();
    // checkBalance();
    updateMainScreenLabel();
  }
  Serial.print("Free heap (updateBurnText end): ");
  Serial.println(ESP.getFreeHeap());
}

const int INHIBIT_START = 131;
const int UNINHIBIT_START = 151;

/**
 * @brief Uninhibit all bill channels so the acceptor accepts mixed currencies.
 * Call from createMainScreen for mixed-currency mode.
 */
void uninhibitAllChannels() {
#if BILL_ACCEPTOR_ENABLED
  int totalChannels = (int)billAmountIntOne.size();
  if (totalChannels <= 0 || totalChannels > 16)
    return;
  Serial.println("NV10: uninhibiting all channels (mixed currency)");
  for (int i = 0; i < totalChannels; i++) {
    SerialPort1.write(UNINHIBIT_START + i);
    delay(25);
  }
  delay(100);
  sessionState.allowedChannelStart = 0;
  sessionState.allowedChannelCount = totalChannels;
  Serial.println("NV10: all channels enabled");
#endif
}

/**
 * Sets the currency to the specified value.
 * @param newCurrency The new currency to set.
 */
/**
 * @brief Sets the currency and configures bill acceptor channels.
 *
 * @param newCurrency The currency to set (currencyOne, currencyTwo, or
 * currencyThree)
 * @param skipInhibit If true, skip the inhibit/uninhibit process (useful at
 * startup when acceptor is off)
 */
void setCurrency(const char *newCurrency, bool skipInhibit = false) {
  Serial.print("setCurrency Currency set to ");
  Serial.println(newCurrency);
  strlcpy(currencySelected, newCurrency, sizeof(currencySelected));

  // Always update allowed range for software filter (button path or bill path)
  int startCh = 0;
  int sizeCh = 0;
  if (strcmp(currencySelected, currencyOne) == 0) {
    startCh = 0;
    sizeCh = originalSizeOne;
  } else if (strcmp(currencySelected, currencyTwo) == 0) {
    startCh = originalSizeOne;
    sizeCh = originalSizeTwo;
  } else if (strcmp(currencySelected, currencyThree) == 0) {
    startCh = originalSizeOne + originalSizeTwo;
    sizeCh = originalSizeThree;
  }
  sessionState.allowedChannelStart = startCh;
  sessionState.allowedChannelCount = sizeCh;

  if (skipInhibit) {
    Serial.println("setCurrency: Skipping inhibit (acceptor not enabled)");
    return;
  }

  int startChannel = startCh;
  int currencySize = sizeCh;

#if BILL_ACCEPTOR_ENABLED
  Serial.println("NV10: inhibiting all channels (131..146)");
  for (int i = 0; i < 16; i++) {
    SerialPort1.write(INHIBIT_START + i);
    delay(100);
  }
  delay(100);
  Serial.println("NV10: all channels inhibited");

  Serial.print("NV10: uninhibiting channels ");
  Serial.print(startChannel);
  Serial.print("..");
  Serial.print(startChannel + currencySize - 1);
  Serial.print(" for ");
  Serial.println(currencySelected);
  for (int i = 0; i < currencySize; i++) {
    int channelCode = UNINHIBIT_START + startChannel + i;
    Serial.print("  allow ");
    Serial.print(currencySelected);
    Serial.print(": ");
    Serial.println(channelCode);
    SerialPort1.write(channelCode);
    delay(200);
  }
  delay(100);
  Serial.println("NV10: channel setup done");
#endif
}

void checkPriceKraken();
void checkPriceCoinGeckoApi();
void checkPrice() {
  if (strcmp(deviceState.rateSourceBuffer, "CoinGecko") == 0) {
    checkPriceCoinGeckoApi();
  } else if (strcmp(deviceState.rateSourceBuffer, "ExchangeApi") == 0) {
    checkPriceExchangeApi();
  } else if (strcmp(deviceState.rateSourceBuffer, "Kraken") == 0) {
    checkPriceKraken();
  } else if (strcmp(deviceState.rateSourceBuffer, "Coingecko") == 0 ||
             strcmp(deviceState.rateSourceBuffer, "CoinYEP") == 0) {
    checkPriceCoinGecko();  // CoinYEP fallback
  } else {
    checkPriceCoinGeckoApi();  // default to CoinGecko
  }
}

void checkPriceCoinGecko() {
  String targetCurrency = currencySelected;
  targetCurrency.toUpperCase();
  beginNetwork(http, secureClient, String(coinyepConversionAPI) + targetCurrency);

  int httpCode = http.GET(); // Send the request

  if (httpCode == 200 || httpCode == 201) // Check the returning code
  {
    String responsePayload =
        http.getString(); // Get the request response payload
    // Serial.println(responsePayload);
    // Parse JSON from CoinYEP. We keep the old function name for backward
    // compatibility with saved "Coingecko" config values.
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, responsePayload);
    if (!error) {
      const char *priceStr = doc["price"] | "";
      fiatValue = String(priceStr).toFloat();
      Serial.print(F("HTTP (checkPriceCoinGecko/CoinYEP): "));
      Serial.println(httpCode);
      Serial.print("CoinYEP raw price: ");
      Serial.println(priceStr);
      Serial.print("BTC/");
      Serial.print(targetCurrency);
      Serial.print(": ");
      Serial.println(fiatValue, 2);
    } else {
      Serial.print("deserializeJson() failed in CoinYEP parser: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print(F("Error (checkPriceCoinGecko/CoinYEP): "));
    Serial.println(httpCode);
  }
  Serial.print("Free heap (checkPriceCoinGecko/CoinYEP): ");
  Serial.println(ESP.getFreeHeap());
  http.end(); // Close connection
}

void checkPriceExchangeApi() {
  beginNetwork(http, secureClient, exchangeapiConversionAPI);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200 || httpResponseCode == 201) {
    String responsePayload = http.getString();
    Serial.println(responsePayload);

    DynamicJsonDocument doc(
        16384); // Increased buffer size for large JSON response
    DeserializationError error = deserializeJson(doc, responsePayload);

    String tempCurrency = currencySelected;
    tempCurrency.toLowerCase();

    if (!error) {
      String date = doc["date"];
      fiatValue = doc["btc"][tempCurrency];

      if (!fiatValue) {
        Serial.print("Error: Rate not found for the specified currency");
        Serial.println(currencySelected);
      } else {
        Serial.println("Date: " + date);
        Serial.print("Exchange Rate for BTC to ");
        Serial.print(currencySelected);
        Serial.print(": ");
        Serial.println(fiatValue, 6);
      }
    } else {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error in HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

static const char *krakenTickerAPI = "https://api.kraken.com/0/public/Ticker";

void checkPriceCoinGeckoApi() {
  String curr = String(currencySelected);
  curr.toLowerCase();
  beginNetwork(http, secureClient, String(coingeckoAPI) + curr);
  int code = http.GET();
  if (code == 200 || code == 201) {
    String payload = http.getString();
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, payload) == DeserializationError::Ok &&
        doc["bitcoin"][curr]) {
      fiatValue = doc["bitcoin"][curr].as<float>();
      Serial.print("CoinGecko BTC/");
      Serial.print(currencySelected);
      Serial.print(": ");
      Serial.println(fiatValue, 2);
    }
  }
  http.end();
}

void checkPriceKraken() {
  String pair = "XBTEUR"; // default
  String curr = String(currencySelected);
  curr.toUpperCase();
  if (curr == "EUR")
    pair = "XBTEUR";
  else if (curr == "USD")
    pair = "XBTUSD";
  else if (curr == "CZK")
    pair = "XBTCZK";
  else if (curr == "GBP")
    pair = "XBTGBP";
  else
    pair = "XBT" + curr;

  beginNetwork(http, secureClient, String(krakenTickerAPI) + "?pair=" + pair);
  int code = http.GET();
  if (code == 200 || code == 201) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, payload) == DeserializationError::Ok &&
        doc["result"] && doc["error"].size() == 0) {
      JsonObject res = doc["result"].as<JsonObject>();
      for (JsonPair kv : res) {
        const char *lastStr = kv.value()["c"][0] | "";
        fiatValue = String(lastStr).toFloat();
        break;
      }
      Serial.print("Kraken BTC/");
      Serial.print(currencySelected);
      Serial.print(": ");
      Serial.println(fiatValue, 2);
    }
  }
  http.end();
}

void createLoadingIndicator() {
  loadingLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(loadingLabel, "Loading...");
  lv_obj_center(loadingLabel);
  lv_obj_set_style_text_font(loadingLabel, &lv_font_montserrat_22,
                             0);                     // Optional: set font size
  lv_obj_add_flag(loadingLabel, LV_OBJ_FLAG_HIDDEN); // Initially hidden
  Serial.println("Loading indicator created");
}

void showLoadingIndicator() {
  lv_obj_clear_flag(loadingLabel, LV_OBJ_FLAG_HIDDEN); // Show loading indicator
  lv_refr_now(NULL); // Force immediate refresh of LVGL
  // delay(100);                                          // Small delay to
  // ensure the display updates
  Serial.println("Loading indicator shown");
}

void hideLoadingIndicator() {
  lv_obj_add_flag(loadingLabel, LV_OBJ_FLAG_HIDDEN); // Hide loading indicator
  lv_refr_now(NULL); // Force immediate refresh of LVGL
  Serial.println("Loading indicator hidden");
}

// Create the main screen
/**
 * @brief Callback function for color animation.
 *
 * This function is called during a color animation and updates the text color
 * of an object. It takes a pointer to the object and an integer value as
 * parameters. The integer value represents the progress of the animation (0 to
 * 255). The function calculates the index in the colors array based on the
 * progress value, and sets the text color of the object to the corresponding
 * color from the array.
 *
 * @param var Pointer to the object.
 * @param v Integer value representing the progress of the animation.
 */
void color_anim_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  int num_colors = sizeof(colors) / sizeof(colors[0]);

  int idx =
      (v * num_colors) /
      256; // This will convert v (0 to 255) to an index in the colors array.

  // Repainting the title on every animation step kept the CPU flushing the
  // framebuffer nonstop, which starved the panel DMA and made the picture
  // twitch. Repaint only when the color actually changes.
  static int lastIdx = -1;
  if (idx == lastIdx) {
    return;
  }
  lastIdx = idx;

  lv_color_t color = colors[idx];
  lv_obj_set_style_text_color(obj, color, 0);
}

/**
 * @brief Updates the main screen label with the current balance, fiat value,
 * and charge value.
 *
 * This function checks if the balanceValueLabel, fiatValueLabel, and
 * chargeValueLabel have been created and initialized. If the WiFi status is
 * offline, it sets the labels to display "OFFLINE" and changes the text color
 * to red. Otherwise, it formats and sets the text of the labels with the
 * appropriate values and changes the text color accordingly. The
 * balanceValueLabel text color is set to white by default, but if the fiat
 * balance is less than the maximum amount selected, it changes the text color
 * to red.
 *
 * @note The labels must be created and initialized before calling this
 * function.
 */
static bool acceptorArmed = false;
// updateMainScreenLabel() also runs while the settings portal is up; the
// acceptor must only open once the customer main screen exists.
static bool mainScreenShown = false;

// One unit of a currency such as PYG is worth a fraction of a sat, so a fixed
// 100-cent probe never succeeds and the acceptor stays dark. Probe the
// smallest configured note instead; that is what a customer can actually insert.
static bool currencyQuoted(const char *currency, int channelStart, int channelCount,
                           float price, float fee) {
  if (currency == nullptr || currency[0] == '\0' || channelCount <= 0) {
    return false;
  }
  int64_t smallestCents = 0;
  for (int i = 0; i < channelCount; i++) {
    const int amount = billAmountIntOne[channelStart + i];
    if (amount <= 0) {
      continue;
    }
    const int64_t cents = (int64_t)amount * 100;
    if (smallestCents == 0 || cents < smallestCents) {
      smallestCents = cents;
    }
  }
  return smallestCents > 0 && quoteSats(smallestCents, price, fee).ok;
}

// The main screen uninhibits every configured channel, so a bill of any
// configured currency is credited. Every one of those currencies needs a
// usable price before the acceptor may open — otherwise a bill whose currency
// has no rate would be taken and the payout would fail with the cash inside.
static bool allCreditableQuotesReady() {
  if (originalSizeOne == 0) {
    return false;
  }
  if (!currencyQuoted(currencyOne, 0, originalSizeOne, sessionState.fiatValue1,
                      charge1)) {
    return false;
  }
  if (originalSizeTwo > 0 &&
      !currencyQuoted(currencyTwo, originalSizeOne, originalSizeTwo,
                      sessionState.fiatValue2, charge2)) {
    return false;
  }
  if (originalSizeThree > 0 &&
      !currencyQuoted(currencyThree, originalSizeOne + originalSizeTwo,
                      originalSizeThree, sessionState.fiatValue3, charge3)) {
    return false;
  }
  return true;
}

// lv_label_set_text() and style writes invalidate and redraw even when the
// content is already on screen, and every redraw is a framebuffer flush that
// can tear against the panel scanout. Skip writes that change nothing.
static void setLabelTextIfChanged(lv_obj_t *label, const char *text) {
  if (label == nullptr) {
    return;
  }
  const char *current = lv_label_get_text(label);
  if (current != nullptr && strcmp(current, text) == 0) {
    return;
  }
  lv_label_set_text(label, text);
}

static void setLabelColorIfChanged(lv_obj_t *label, lv_color_t color) {
  if (label == nullptr) {
    return;
  }
  if (lv_obj_get_style_text_color(label, LV_PART_MAIN).full != color.full) {
    lv_obj_set_style_text_color(label, color, 0);
  }
}

// Sends a channel mask to the NV10: one inhibit or uninhibit per channel, then
// the master enable. An empty mask is a master inhibit. Bit i is channel i.
// The mask is sent only when it changes. `limitValue` and `unit` are only for
// the log line.
static void applyNoteMask(uint16_t mask, long limitValue, const char *unit) {
  const int count = (int)billAmountIntOne.size();
  const int limit = count < 16 ? count : 16;
  if ((uint32_t)mask == enabledNoteMask) {
    return;
  }
  enabledNoteMask = mask;
  noteMaskOwnsWrite = true;
  if (mask == 0) {
    billAcceptorWrite(185);
    noteMaskOwnsWrite = false;
    Serial.printf("NV10: no note fits in %ld %s\n", limitValue, unit);
    return;
  }
  for (int i = 0; i < limit; i++) {
    const uint8_t cmd = (mask & (uint16_t)(1u << i))
                            ? (uint8_t)(UNINHIBIT_START + i)
                            : (uint8_t)(INHIBIT_START + i);
    billAcceptorWrite(cmd);
    delay(25);
  }
  billAcceptorWrite(184);
  noteMaskOwnsWrite = false;
  Serial.printf("NV10: notes open for %ld %s (mask 0x%04x)\n", limitValue, unit,
                mask);
}

// Enable only the notes that fit in `room`. A larger note is inhibited on its
// own channel, so the NV10 returns it, and any smaller note still in range
// stays open.
static void applyFittingNotes(long room) {
  uint16_t mask = 0;
  const int count = (int)billAmountIntOne.size();
  const int limit = count < 16 ? count : 16;
  for (int i = 0; i < limit; i++) {
    const int amount = billAmountIntOne[i];
    if (amount > 0 && (long)amount <= room) {
      mask |= (uint16_t)(1u << i);
    }
  }
  applyNoteMask(mask, room, currencySelected);
}

// Offline LNbits pays out through a stored link and cannot read the wallet, so
// the wallet limit below does not apply there.
static bool offlineLnbitsMode() {
  return strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0 &&
         !wifiStatus();
}

// The funding wallet in whole sats. LNbits reports millisats, Stablesats and
// Flash report dollar cents.
static bool currentWalletSats(int64_t *out) {
  BalanceUnit unit = BalanceUnit::Sats;
  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    unit = BalanceUnit::Msats;
  } else if (fundingWalletIsUsd()) {
    unit = BalanceUnit::UsdCents;
  }
  return walletBalanceSats(unit, (int64_t)balanceSats,
                           (double)sessionState.btcUsdValue, out);
}

// Several currencies share one NV10 channel list: the first originalSizeOne
// channels are currency one, the next originalSizeTwo currency two, the rest
// currency three. A channel is open only when the cash already inserted plus
// that note still quotes to sats the wallet holds.
static uint16_t mixedNoteMask(int64_t walletSats) {
  NoteChannel channels[16];
  const int count = (int)billAmountIntOne.size() < 16
                        ? (int)billAmountIntOne.size()
                        : 16;
  const int firstEnd = (int)originalSizeOne;
  const int secondEnd = (int)(originalSizeOne + originalSizeTwo);
  for (int i = 0; i < count; i++) {
    channels[i].leg = i < firstEnd ? 0 : (i < secondEnd ? 1 : 2);
    channels[i].amount = billAmountIntOne[i];
  }
  const MixedLeg legs[3] = {
      {sessionState.totalCurrency1, sessionState.fiatValue1, charge1},
      {sessionState.totalCurrency2, sessionState.fiatValue2, charge2},
      {sessionState.totalCurrency3, sessionState.fiatValue3, charge3},
  };
  return fittingNoteMask(channels, count, legs, walletSats);
}

static void showMainHold(const char *text) {
  if (mainScreenHoldLabel == nullptr) {
    return;
  }
  setLabelTextIfChanged(mainScreenHoldLabel, text);
  if (lv_obj_has_flag(mainScreenHoldLabel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_clear_flag(mainScreenHoldLabel, LV_OBJ_FLAG_HIDDEN);
  }
}

static void hideMainHold() {
  if (mainScreenHoldLabel != nullptr &&
      !lv_obj_has_flag(mainScreenHoldLabel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(mainScreenHoldLabel, LV_OBJ_FLAG_HIDDEN);
  }
}

static void armAcceptorIfQuoted() {
  if (!mainScreenShown) {
    return;
  }
  // Inserting owns its own channel mask. A sale (QR, paying, thank-you,
  // error) must not take another note; the price refresh used to reopen
  // the acceptor behind those screens. It opens again on the main screen.
  if (currentUiState != UI_IDLE) {
    return;
  }
  // A note that clears the sensor is stacked. Notes larger than the offset
  // balance are inhibited one channel at a time; smaller notes stay open.
  if (!offlineLnbitsMode() && !mixedCurrenciesConfigured()) {
    if (paymentService.isGaloy(deviceState.fundingSourceBuffer) && !wifiStatus()) {
      applyFittingNotes(0);
      acceptorArmed = false;
      return;
    }
    if (!allCreditableQuotesReady()) {
      return;
    }
    const long room = insertSessionCap();
    const int smallestNote = smallestConfiguredNote();
    const bool known = balanceSats > 0 && fiatBalance > 0.0f && smallestNote > 0;
    const bool covers = known && room >= (long)smallestNote;
    if (!covers) {
      applyFittingNotes(0);
      acceptorArmed = false;
      showMainHold(known ? "Balance too low" : "Waiting for price and balance");
      Serial.printf("NV10 stays off: balance %ld %s, smallest note %d\n",
                    room, currencySelected, smallestNote);
      return;
    }
    hideMainHold();
    applyFittingNotes(room);
    acceptorArmed = true;
    return;
  }
  if (!offlineLnbitsMode()) {
    // Several currencies. Same rule as above, counted in sats because the
    // notes are priced in different currencies: a channel stays open only
    // while the wallet can pay what is inserted plus that note.
    if (paymentService.isGaloy(deviceState.fundingSourceBuffer) && !wifiStatus()) {
      applyNoteMask(0, 0, "sats");
      acceptorArmed = false;
      return;
    }
    if (!allCreditableQuotesReady()) {
      Serial.printf(
          "NV10 stays off: sizes %d/%d/%d prices %.2f/%.2f/%.2f\n",
          originalSizeOne, originalSizeTwo, originalSizeThree,
          sessionState.fiatValue1, sessionState.fiatValue2,
          sessionState.fiatValue3);
      return;
    }
    int64_t walletSats = 0;
    const bool known = currentWalletSats(&walletSats);
    const uint16_t mask = known ? mixedNoteMask(walletSats) : 0;
    if (mask == 0) {
      applyNoteMask(0, (long)walletSats, "sats");
      acceptorArmed = false;
      showMainHold(known ? "Balance too low" : "Waiting for price and balance");
      Serial.printf("NV10 stays off: wallet %lld sats\n", (long long)walletSats);
      return;
    }
    hideMainHold();
    applyNoteMask(mask, (long)walletSats, "sats");
    acceptorArmed = true;
    return;
  }
  if (acceptorArmed) {
    return;
  }
  // Offline LNbits pays out through the stored LNURL-device link, which
  // encodes the fiat amount directly - no BTC price is needed, and the wallet
  // cannot be read, so the quote and wallet gates are skipped.
  uninhibitAllChannels();
  enableAcceptor();
  acceptorArmed = true;
}

void updateMainScreenLabel() {
  Serial.print("Free heap (updateMainScreenLabel Start): ");
  Serial.println(ESP.getFreeHeap());
  if (balanceValueLabel) { // Ensure the label has been created
    if (!wifiStatus()) {
      setLabelTextIfChanged(balanceValueLabel, "OFFLINE");
    } else {
      char buffer[32];
      // Customer balance is the insertable amount: wallet after the fee,
      // rounded down to the smallest note. The raw wallet stays in fiatBalance.
      const long usable = insertSessionCap();
      snprintf(buffer, sizeof(buffer), "%ld %s", usable, currencySelected);
      setLabelTextIfChanged(balanceValueLabel, buffer);
      setLabelColorIfChanged(balanceValueLabel,
                             usable > 0 ? LV_COLOR_WHITE : LV_COLOR_RED);
    }
  }
  if (fiatValueLabel) { // Check if it has been initialized
    if (!wifiStatus()) {
      setLabelTextIfChanged(fiatValueLabel, "OFFLINE");
      setLabelColorIfChanged(fiatValueLabel, LV_COLOR_RED);
    } else {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%ld %s", (long)fiatValue,
               currencySelected);
      setLabelTextIfChanged(fiatValueLabel, buffer);
      setLabelColorIfChanged(fiatValueLabel, LV_COLOR_GREEN);
    }
  }
  if (chargeValueLabel) { // Check if it has been initialized
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f %%", (double)chargeSelected);
    setLabelTextIfChanged(chargeValueLabel, buffer);
  }
  if (mainScreenCurrency1RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue1 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue1);
    else
      snprintf(buf, sizeof(buf), "-");
    setLabelTextIfChanged(mainScreenCurrency1RateLabel, buf);
  }
  if (mainScreenCurrency1FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge1);
    setLabelTextIfChanged(mainScreenCurrency1FeeLabel, feeBuf);
  }
  if (mainScreenCurrency2RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue2 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue2);
    else
      snprintf(buf, sizeof(buf), "-");
    setLabelTextIfChanged(mainScreenCurrency2RateLabel, buf);
  }
  if (mainScreenCurrency2FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge2);
    setLabelTextIfChanged(mainScreenCurrency2FeeLabel, feeBuf);
  }
  if (mainScreenCurrency3RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue3 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue3);
    else
      snprintf(buf, sizeof(buf), "-");
    setLabelTextIfChanged(mainScreenCurrency3RateLabel, buf);
  }
  if (mainScreenCurrency3FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge3);
    setLabelTextIfChanged(mainScreenCurrency3FeeLabel, feeBuf);
  }

  Serial.print("Free heap (updateMainScreenLabel End): ");
  Serial.println(ESP.getFreeHeap());
  armAcceptorIfQuoted();
}

/**
 * @brief Creates the main screen of the ATM.
 *
 * This function initializes and configures various UI elements such as labels,
 * images, and buttons to create the main screen of the ATM. It sets the text,
 * alignment, and font styles of the labels, and loads the screen onto the
 * display. It also handles the creation of additional UI elements based on
 * certain conditions, such as the presence of animated text or specific
 * subtitle values.
 *
 * @note This function assumes that the necessary LVGL library and display
 * configurations have been properly set up beforehand.
 */
void createMainScreen() {
  lv_task_handler();
  if (INHIBITMECH >= 0) {
    digitalWrite(INHIBITMECH, LOW);
  }

  Serial.println("createMainScreen: Start machine");
  Serial.print("Free heap (createMainScreen Start): ");
  Serial.println(ESP.getFreeHeap());

  // Build the screen off-screen and load it once. Loading it empty and then
  // filling the labels draws the panel twice, which shows as a flicker.
  screen_main = lv_obj_create(NULL); // Create a new screen
  Serial.println("createMainScreen: Screen created");

  lv_obj_t *verLabel = lv_label_create(screen_main);
  lv_label_set_text(verLabel, "v" FW_VERSION);
  lv_obj_align(verLabel, LV_ALIGN_TOP_RIGHT, -15, 10);
  lv_obj_set_style_text_font(verLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(verLabel, lv_color_hex(0x808080), 0);

  String LVGL_Atm_desc = "BITCOIN LIGHTNING ATM ";
  if (atmdesc[0] != '\0') {
    LVGL_Atm_desc = atmdesc;
  };
  lv_obj_t *label = lv_label_create(screen_main);  // full screen as the parent
  lv_label_set_text(label, LVGL_Atm_desc.c_str()); // set label text
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);    // Center but 20 from the top
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);

  String LVGL_Zero_Title = "";
  if (atmsubtitle[0] != '\0') {
    LVGL_Zero_Title = atmsubtitle;
  }
  if (strcmp(atmsubtitle, "DVADSATJEDEN") == 0 ||
      strcmp(atmsubtitle, "Dvadsatjeden") == 0 ||
      strcmp(atmsubtitle, "21") == 0) {
    LVGL_Zero_Title = "DVADSATJEDEN";
  }
  lv_obj_t *zeroline =
      lv_label_create(screen_main); // full screen as the parent
  lv_label_set_text(zeroline, LVGL_Zero_Title.c_str()); // set label text
  lv_obj_align(zeroline, LV_ALIGN_TOP_MID, 0, 60); // Center but 20 from the top
  if (strcmp(atmsubtitle, "AMITY") == 0 || strcmp(atmsubtitle, "Amity") == 0) {
    lv_label_set_text(zeroline, "");
  }
  if (strcmp(atmsubtitle, "DVADSATJEDEN") == 0 ||
      strcmp(atmsubtitle, "Dvadsatjeden") == 0 ||
      strcmp(atmsubtitle, "21") == 0) {
    lv_obj_set_style_text_font(
        zeroline, &lv_font_the_bold_48,
        0); // Assuming lv_font_montserrat_22 is a bold font.
  } else {
    lv_obj_set_style_text_font(zeroline, &lv_font_montserrat_48, 0);
  }

  String LVGL_Fiat_Hell = "FIAT HELL";
  if (atmtitle[0] != '\0') {
    LVGL_Fiat_Hell = atmtitle;
  };
  lv_obj_t *fiathell =
      lv_label_create(screen_main); // full screen as the parent
  lv_label_set_text(fiathell, LVGL_Fiat_Hell.c_str()); // set label text
  lv_obj_align(fiathell, LV_ALIGN_TOP_MID, 0, 125); // Center but 95 from the top
  lv_obj_set_style_text_font(
      fiathell, &lv_font_montserrat_bold_60,
      0); // Assuming lv_font_montserrat_22 is a bold font.

  if (strcmp(animated, "Yes") == 0) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fiathell);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_time(&a, 500); // one sweep through the palette
    // ...and the same time back, so the color pulses instead of snapping
    // from the last color to the first twice per second.
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_exec_cb(&a, color_anim_cb);
    lv_anim_start(&a);
    Serial.println("createMainScreen: Animation started");
  } else {
    lv_obj_set_style_text_color(fiathell, LV_COLOR_ORANGE, 0);
  }

  /* Create a label with big text */
  burnTextLabel = lv_label_create(screen_main); // Assign it to global variable
  String combinedText = "BURN YOUR SHITCOIN FOR SATS";
  lv_label_set_text(burnTextLabel, combinedText.c_str());
  lv_obj_set_style_text_font(burnTextLabel, &lv_font_montserrat_24,
                             0); // Use the large font
  lv_obj_align(burnTextLabel, LV_ALIGN_TOP_MID, 0,
               223); // Center but 163 from the top
  Serial.println("createMainScreen: burnTextLabel created");

  if (strcmp(atmsubtitle, "DVADSATJEDEN") == 0 || 
      strcmp(atmsubtitle, "Dvadsatjeden") == 0 ||
      strcmp(atmsubtitle, "21") == 0) {
    lv_obj_t *img1 = lv_img_create(screen_main); // Create an image object
    lv_img_set_src(img1, &btcSmallImg);          // Set the image source to your
                                                 // converted image (my_image)
    lv_obj_align(img1, LV_ALIGN_TOP_MID, 180,
                 70); // Align the image to the center of the screen
    Serial.println("createMainScreen: btc logo added");
    }

  if (strcmp(atmsubtitle, "AMITY") == 0 || strcmp(atmsubtitle, "Amity") == 0) {
    lv_obj_t *img1 = lv_img_create(screen_main); // Create an image object
    lv_img_set_src(
        img1,
        &amityImg); // Set the image source to your converted image (my_image)
    lv_obj_align(img1, LV_ALIGN_TOP_MID, 0,
                 15); // Align the image to the center of the screen
    Serial.println("createMainScreen: amity logo added");
  }

  lv_obj_t *labelBalance =
      lv_label_create(screen_main);           // full screen as the parent
  lv_label_set_text(labelBalance, "BALANCE"); // set label text
  lv_obj_align(labelBalance, LV_ALIGN_BOTTOM_LEFT, 30,
               -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelBalance, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelBalance created");

  lv_obj_t *labelPrice =
      lv_label_create(screen_main);       // full screen as the parent
  lv_label_set_text(labelPrice, "PRICE"); // set label text
  lv_obj_align(labelPrice, LV_ALIGN_BOTTOM_MID, 0,
               -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelPrice, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelPrice created");

  lv_obj_t *labelCharge =
      lv_label_create(screen_main);      // full screen as the parent
  lv_label_set_text(labelCharge, "FEE"); // set label text
  lv_obj_align(labelCharge, LV_ALIGN_BOTTOM_RIGHT, -30,
               -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelCharge, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelCharge created");

  char buffer[32];
  snprintf(
      buffer, sizeof(buffer), "%ld %s", insertSessionCap(),
      currencySelected);
  balanceValueLabel = lv_label_create(screen_main); // full screen as the parent
  lv_label_set_text(
      balanceValueLabel,
      buffer); // set label text now that balanceValueLabel is created
  lv_obj_align(balanceValueLabel, LV_ALIGN_BOTTOM_LEFT, 30,
               -20); // Center but 20 from the top
  lv_obj_set_style_text_font(balanceValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: balanceValueLabel created");

  snprintf(buffer, sizeof(buffer), "%ld %s", (long)fiatValue,
           currencySelected); // Display as whole number and append the currency
  fiatValueLabel =
      lv_label_create(screen_main);          // Create it on your main screen
  lv_label_set_text(fiatValueLabel, buffer); // Initial text
  lv_obj_align(fiatValueLabel, LV_ALIGN_BOTTOM_MID, 0,
               -20); // Position it as you like
  lv_obj_set_style_text_font(fiatValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: fiatValueLabel created");

  snprintf(buffer, sizeof(buffer), "%.1f %%",
           (double)chargeSelected);
  chargeValueLabel =
      lv_label_create(screen_main); // Create it on your main screen
  lv_label_set_text(chargeValueLabel,
                    buffer); // Set the label text using the buffer
  lv_obj_align(chargeValueLabel, LV_ALIGN_BOTTOM_RIGHT, -30,
               -20); // Position it as you like
  lv_obj_set_style_text_font(chargeValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: chargeValueLabel created");

  createAcceptedCurrenciesSection();
  Serial.println("createMainScreen: accepted currencies section created");
  //}
  img_blink = lv_img_create(screen_main);
  lv_img_set_src(
      img_blink,
      &blink); // 'blink' must be a properly defined LVGL image variable
  lv_obj_align(img_blink, LV_ALIGN_TOP_RIGHT, -10, 35);
  lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);

  img_lnbits = lv_img_create(screen_main);
  lv_img_set_src(
      img_lnbits,
      &lnbits); // 'lnbits' must be a properly defined LVGL image variable
  lv_obj_align(img_lnbits, LV_ALIGN_TOP_RIGHT, -10, 35);
  lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);

  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
  } else if (strcmp(deviceState.fundingSourceBuffer, "Blink") == 0) {
    lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
  } else if (strcmp(deviceState.fundingSourceBuffer, "Flash") == 0) {
    lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *flashLabel = lv_label_create(screen_main);
    lv_label_set_text(flashLabel, "FLASH");
    lv_obj_set_style_text_font(flashLabel, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(flashLabel, lv_color_hex(0xFF9900), 0);
    lv_obj_align(flashLabel, LV_ALIGN_TOP_RIGHT, -10, 35);
  }

  mainScreenHoldLabel = lv_label_create(screen_main);
  lv_label_set_text(mainScreenHoldLabel, "");
  lv_obj_set_width(mainScreenHoldLabel, 720);
  lv_label_set_long_mode(mainScreenHoldLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(mainScreenHoldLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(mainScreenHoldLabel, LV_ALIGN_CENTER, 0, 20);
  lv_obj_set_style_text_font(mainScreenHoldLabel, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(mainScreenHoldLabel, LV_COLOR_RED, 0);
  lv_obj_add_flag(mainScreenHoldLabel, LV_OBJ_FLAG_HIDDEN);

  // Fill the labels before the screen is shown, so the panel draws once.
  // updateMainScreenLabel() arms the acceptor, which needs mainScreenShown.
  mainScreenShown = true;
  updateMainScreenLabel();
  lv_scr_load(screen_main);
  attachBatteryToCurrentScreen();
  Serial.println("createMainScreen: Screen loaded");
  Serial.print("Free heap (createMainScreen End): ");
  Serial.println(ESP.getFreeHeap());
}

void enableAcceptor() {
  if (paymentService.isGaloy(deviceState.fundingSourceBuffer) &&
      (!wifiStatus())) {
    Serial.println(
        "Error: online funding source selected but the device is offline");
    return;
  }
  billAcceptorWrite(184);  // Enable acceptor (channels already set by setCurrency)
  Serial.println("NV10: master enable 184");
  if (INHIBITMECH >= 0) {
    digitalWrite(INHIBITMECH, HIGH); // Uninhibit currencies
  }
}

static void discardInsertMoneyScreen() {
  uiController.deleteInsertMoneyScreen();
  labelLastInserted = nullptr;
  labelTotalAmount = nullptr;
  labelTotalCurrency1 = nullptr;
  labelTotalCurrency2 = nullptr;
  labelTotalCurrency3 = nullptr;
  labelTotalSats = nullptr;
  labelMaxAmount = nullptr;
  insertFinishBtn = nullptr;
}

// Blank screen with a spinner, shown from FINISH until the QR (or an error)
// is ready. The payout request takes seconds, and a screen that does not
// change looks like the press was ignored.
static lv_obj_t *screen_wait = nullptr;
static lv_obj_t *waitCaption = nullptr;

// `caption` is optional text under the spinner; payouts pass none.
static void showPayoutWaitScreen(const char *caption = nullptr) {
  if (screen_wait != nullptr) {
    lv_label_set_text(waitCaption, caption != nullptr ? caption : "");
    lv_scr_load(screen_wait);
    attachBatteryToCurrentScreen();
    lv_refr_now(NULL);
    return;
  }
  screen_wait = lv_obj_create(NULL);
  lv_obj_t *spinner = lv_spinner_create(screen_wait, 1000, 70);
  lv_obj_set_size(spinner, 110, 110);
  lv_obj_center(spinner);
  styleWaitSpinner(spinner);
  waitCaption = lv_label_create(screen_wait);
  lv_label_set_text(waitCaption, caption != nullptr ? caption : "");
  lv_obj_set_style_text_font(waitCaption, &lv_font_montserrat_22, 0);
  lv_obj_align(waitCaption, LV_ALIGN_CENTER, 0, 100);
  lv_scr_load(screen_wait);
  attachBatteryToCurrentScreen();
  lv_refr_now(NULL);
}

// Delete only after the next screen is active and owns the battery widget.
static void discardPayoutWaitScreen() {
  if (screen_wait == nullptr) {
    return;
  }
  attachBatteryToCurrentScreen();
  if (lv_scr_act() != screen_wait) {
    lv_obj_del(screen_wait);
    screen_wait = nullptr;
    waitCaption = nullptr;
  }
}

// Builds the QR payload. The network call runs on a worker task while the
// main loop keeps the spinner turning; the worker is the only user of the
// HTTP clients meanwhile.
struct PayoutJob {
  volatile bool done;
  bool blinkFlow;
  const char *payload;
};

static void buildPayoutPayload(PayoutJob *job) {
  job->blinkFlow = false;
  job->payload = nullptr;
  if (!wifiStatus()) {
    job->payload = makeLNURL() ? qrData : nullptr;
  } else if (paymentService.isGaloy(deviceState.fundingSourceBuffer)) {
    // Without an LNURL and callback a QR would trap the customer in a
    // polling loop that can never succeed.
    job->payload = createLNURLWithdraw() ? lnURLgen : nullptr;
    job->blinkFlow = true;
  } else if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    if (paymentService.hasLNbitsConfig(lnbitsURL, adminkey, readkey)) {
      job->payload = getLNURL() ? lnURLgen : nullptr;
    } else {
      job->payload = makeLNURL() ? qrData : nullptr;
    }
  }
}

static void payoutWorkerTask(void *arg) {
  PayoutJob *job = static_cast<PayoutJob *>(arg);
  buildPayoutPayload(job);
  // Bytes of this 12 KB stack that were never touched. A TLS handshake here
  // overflowed 8 KB on the loop task, so keep an eye on the margin.
  Serial.printf("payout task: %u stack bytes never used\n",
                (unsigned)uxTaskGetStackHighWaterMark(NULL));
  job->done = true;
  vTaskDelete(NULL);
}

static void runPayoutRequest(PayoutJob *job) {
  job->done = false;
  if (xTaskCreate(payoutWorkerTask, "payout", 12288, job, 1, NULL) != pdPASS) {
    Serial.println("payout worker not started; running inline");
    buildPayoutPayload(job);
    job->done = true;
    return;
  }
  while (!job->done) {
    lv_task_handler();
    delay(5);
  }
}

// Paying the customer's invoice takes seconds too. Same approach: worker
// task for the request, spinner turning meanwhile.
struct BlinkPayJob {
  volatile bool done;
  bool ok;
};

static void blinkPayWorkerTask(void *arg) {
  BlinkPayJob *job = static_cast<BlinkPayJob *>(arg);
  job->ok = getBlinkLnURL(sessionState.boltInvoice);
  Serial.printf("blinkpay task: %u stack bytes never used\n",
                (unsigned)uxTaskGetStackHighWaterMark(NULL));
  job->done = true;
  vTaskDelete(NULL);
}

static bool runBlinkPayment() {
  BlinkPayJob job = {false, false};
  if (xTaskCreate(blinkPayWorkerTask, "blinkpay", 12288, &job, 1, NULL) !=
      pdPASS) {
    Serial.println("pay worker not started; running inline");
    return getBlinkLnURL(sessionState.boltInvoice);
  }
  while (!job.done) {
    lv_task_handler();
    delay(5);
  }
  return job.ok;
}

// The acceptor is already inhibited on every path that gets here.
static void enterPaymentError() {
  createPaymentErrorScreen();
  lv_task_handler();
  currentUiState = UI_PAYMENT_ERROR;
  stateEnterTime = millis();
}

/**
 * @brief Creates the insert money screen.
 *
 * This function creates a new screen and adds labels for displaying the money
 * inserted, total amount, prompt, and maximum amount. It also sets the
 * necessary styles for the labels.
 * @note This function assumes that the main screen has already been deleted and
 * the global variable `isInsertingMoney` has been set to `true`.
 * @note This function prints the free heap size to the serial monitor.
 */
static void insertFinishCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    insertFinishTapped = true;
  }
}

// Smallest configured note. The on-screen max is rounded down to it so the
// number is a total the customer can actually reach.
static int smallestConfiguredNote() {
  int smallest = 0;
  for (int amount : billAmountIntOne) {
    if (amount > 0 && (smallest == 0 || amount < smallest)) {
      smallest = amount;
    }
  }
  return smallest;
}

// Transaction ceiling in whole currency units. The configured max wins when
// the wallet can pay it. Otherwise the ceiling is what the wallet can pay
// after the fee. Rounded down to the smallest note so the customer sees a
// total they can actually make. The largest-note safety rule is not printed.
static long insertSessionCap() {
  double walletFiat = 0.0;
  const bool haveWallet = displayedFiatBalance(&walletFiat);
  const double net = (100.0 - (double)chargeSelected) / 100.0;
  double cap = (haveWallet && net > 0.0) ? walletFiat / net : 0.0;
  const bool fromWallet = haveWallet && cap > 0.0;
  if (maxamountSelected > 0.0f && (!fromWallet || maxamountSelected < cap)) {
    cap = maxamountSelected;
  }
  long shown = (long)floor(cap);
  const int step = smallestConfiguredNote();
  if (step > 0 && shown > 0) {
    shown = (shown / step) * step;
  }
  return shown;
}

static void refreshInsertMaxLabel();

static void refreshInsertSummary(int lastAmount) {
  char buf[96];
  if (labelLastInserted) {
    if (lastAmount > 0) {
      snprintf(buf, sizeof(buf), "Last bill: %d %s", lastAmount,
               currencySelected);
    } else {
      snprintf(buf, sizeof(buf), "Insert notes");
    }
    lv_label_set_text(labelLastInserted, buf);
  }
  if (labelTotalAmount) {
    snprintf(buf, sizeof(buf), "%ld %s", (long)total, currencySelected);
    lv_label_set_text(labelTotalAmount, buf);
  }
  if (labelTotalSats) {
    const Quote quote =
        quoteSats(llround(total * 100.0), fiatValue, chargeSelected);
    if (total > 0 && quote.ok) {
      snprintf(buf, sizeof(buf), "%lld sats", (long long)quote.sats);
      lv_label_set_text(labelTotalSats, buf);
    } else {
      lv_label_set_text(labelTotalSats, "");
    }
  }
  refreshInsertMaxLabel();
}

// Room still payable, in whole currency units, rounded down to the smallest note.
static long insertRoomLeft(long *capOut) {
  const long cap = insertSessionCap();
  if (capOut != nullptr) {
    *capOut = cap;
  }
  long left = cap - (long)llround((double)total);
  if (left < 0) {
    left = 0;
  }
  const int step = smallestConfiguredNote();
  if (step > 0 && left > 0) {
    left = (left / step) * step;
  }
  return left;
}

// Customer line: how much of the offset balance is still left, in steps of
// the smallest note. Larger notes are inhibited on their own channels, so
// this reaches "max reached" only when the smallest note no longer fits.
static void refreshInsertMaxLabel() {
  if (labelMaxAmount == nullptr || mixedCurrenciesConfigured()) {
    return;
  }
  long cap = 0;
  const long left = insertRoomLeft(&cap);
  const int step = smallestConfiguredNote();
  char buf[96];
  if (cap <= 0) {
    snprintf(buf, sizeof(buf), "Max: —");
  } else if (left <= 0 || (step > 0 && left < (long)step)) {
    snprintf(buf, sizeof(buf), "Max reached — press FINISH");
  } else {
    snprintf(buf, sizeof(buf), "Max: %ld %s left", left, currencySelected);
  }
  setLabelTextIfChanged(labelMaxAmount, buf);
}

void createInsertMoneyScreen() {
  isInsertingMoney = true;
  insertFinishTapped = false;

  Serial.println("Inside createInsertMoneyScreen()");
  Serial.print("Free heap (createInsertMoneyScreen): ");
  Serial.println(ESP.getFreeHeap());

  // Create a new screen
  // lv_obj_t *screen_insert_money = lv_obj_create(NULL);
  screen_insert_money = lv_obj_create(NULL); // Create a new screen
  if (!screen_insert_money) {
    Serial.println("Failed to create a new screen!");
    return;
  }

  // Create label for displaying the last inserted amount
  labelLastInserted = lv_label_create(screen_insert_money);
  if (labelLastInserted) {
    lv_label_set_text(labelLastInserted, "");
    lv_obj_align(labelLastInserted, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_text_font(labelLastInserted, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(labelLastInserted, lv_color_hex(0xC8C8C8), 0);
  } else {
    Serial.println("Failed to create labelLastInserted!");
  }

  // Create label for displaying the total amount (single-currency) or empty (mixed)
  labelTotalAmount = lv_label_create(screen_insert_money);
  if (labelTotalAmount) {
    lv_label_set_text(labelTotalAmount, "");
    lv_obj_align(labelTotalAmount, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_text_font(labelTotalAmount, &lv_font_montserrat_48, 0);
  } else {
    Serial.println("Failed to create labelTotalAmount!");
  }

  // Mixed-currency: one line per currency total
  labelTotalCurrency1 = lv_label_create(screen_insert_money);
  if (labelTotalCurrency1) {
    lv_label_set_text(labelTotalCurrency1, "");
    lv_obj_align(labelTotalCurrency1, LV_ALIGN_TOP_LEFT, 30, 100);
    lv_obj_set_style_text_font(labelTotalCurrency1, &lv_font_montserrat_24, 0);
  }
  labelTotalCurrency2 = lv_label_create(screen_insert_money);
  if (labelTotalCurrency2) {
    lv_label_set_text(labelTotalCurrency2, "");
    lv_obj_align(labelTotalCurrency2, LV_ALIGN_TOP_LEFT, 30, 130);
    lv_obj_set_style_text_font(labelTotalCurrency2, &lv_font_montserrat_24, 0);
  }
  labelTotalCurrency3 = lv_label_create(screen_insert_money);
  if (labelTotalCurrency3) {
    lv_label_set_text(labelTotalCurrency3, "");
    lv_obj_align(labelTotalCurrency3, LV_ALIGN_TOP_LEFT, 30, 160);
    lv_obj_set_style_text_font(labelTotalCurrency3, &lv_font_montserrat_24, 0);
  }

  // Sats the current total will pay.
  labelTotalSats = lv_label_create(screen_insert_money);
  if (labelTotalSats) {
    lv_label_set_text(labelTotalSats, "");
    lv_obj_align(labelTotalSats, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_font(labelTotalSats, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(labelTotalSats, lv_color_hex(0x90EE90), 0);
  }

  // Create label for displaying the maximum amount
  labelMaxAmount = lv_label_create(screen_insert_money);
  if (labelMaxAmount) {
    lv_label_set_text(labelMaxAmount, "");
    lv_obj_align(labelMaxAmount, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_text_font(labelMaxAmount, &lv_font_montserrat_20, 0);
  } else {
    Serial.println("Failed to create labelMaxAmount!");
  }

  insertFinishBtn = lv_btn_create(screen_insert_money);
  lv_obj_set_size(insertFinishBtn, 320, 72);
  lv_obj_align(insertFinishBtn, LV_ALIGN_BOTTOM_MID, 0, -28);
  lv_obj_add_event_cb(insertFinishBtn, insertFinishCb, LV_EVENT_CLICKED,
                      nullptr);
  lv_obj_t *finishLabel = lv_label_create(insertFinishBtn);
  lv_label_set_text(finishLabel, "FINISH");
  lv_obj_set_style_text_font(finishLabel, &lv_font_montserrat_22, 0);
  lv_obj_center(finishLabel);
  styleActionButton(insertFinishBtn);
  refreshInsertSummary(0);

  // Labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_insert_money);
  attachBatteryToCurrentScreen();
}

static void switch_animation_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *switch_obj = lv_event_get_target(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    if (lv_obj_has_state(switch_obj, LV_STATE_CHECKED)) {
      strcpy(deviceState.enableAnimBuffer, "Yes");
    } else {
      strcpy(deviceState.enableAnimBuffer, "No");
    }
    strlcpy(guiConfig.animated, deviceState.enableAnimBuffer,
            sizeof(guiConfig.animated));
    Serial.print("Animation enabled: ");
    Serial.println(deviceState.enableAnimBuffer);
    // Save the updated setting to JSON
    configService.saveGuiConfig(FlashFS, GUI_FILE, guiConfig);
  }
}

/**
 * @brief Creates one currency block (ticker + rate + fee) and returns the rate/fee label pointers via out params.
 */
static void createCurrencyBlock(lv_obj_t *parent, int x, int y, const char *ticker,
    lv_obj_t **outRateLabel, lv_obj_t **outFeeLabel, float feePct) {
  const int block_w = 200;
  const int block_h = 100;
  const int pad = 10;

  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_set_size(cont, block_w, block_h);
  lv_obj_set_pos(cont, x, y);
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_color(cont, LV_COLOR_ORANGE, 0);
  lv_obj_set_style_border_width(cont, 2, 0);
  lv_obj_set_style_radius(cont, 8, 0);
  lv_obj_set_style_pad_all(cont, pad, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *tickerLabel = lv_label_create(cont);
  lv_label_set_text(tickerLabel, ticker);
  lv_obj_set_style_text_font(tickerLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tickerLabel, LV_COLOR_ORANGE, 0);
  lv_obj_align(tickerLabel, LV_ALIGN_TOP_MID, 0, 2);

  *outRateLabel = lv_label_create(cont);
  lv_label_set_text(*outRateLabel, "-");
  lv_obj_set_style_text_font(*outRateLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(*outRateLabel, lv_color_hex(0xE0E0E0), 0);
  lv_obj_align(*outRateLabel, LV_ALIGN_TOP_MID, 0, 38);

  char feeBuf[24];
  snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)feePct);
  *outFeeLabel = lv_label_create(cont);
  lv_label_set_text(*outFeeLabel, feeBuf);
  lv_obj_set_style_text_font(*outFeeLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(*outFeeLabel, lv_color_hex(0xA0A0A0), 0);
  lv_obj_align(*outFeeLabel, LV_ALIGN_TOP_MID, 0, 64);
}

/**
 * @brief Creates the "Accepted currencies" section: 1–3 blocks (ticker + rate + fee), layout by count.
 */
void createAcceptedCurrenciesSection() {
  const int section_y = 262;
  const int block_w = 200;
  const int gap = 24;
  const int total_w_1 = block_w;
  const int total_w_2 = block_w * 2 + gap;
  const int total_w_3 = block_w * 3 + gap * 2;
  const int screen_center = (int)screenWidth / 2;

  int n = 0;
  if (currencyOne[0] != '\0') n++;
  if (currencyTwo[0] != '\0') n++;
  if (currencyThree[0] != '\0') n++;

  int start_x;
  if (n == 1)
    start_x = screen_center - total_w_1 / 2;
  else if (n == 2)
    start_x = screen_center - total_w_2 / 2;
  else
    start_x = screen_center - total_w_3 / 2;

  if (currencyOne[0] != '\0') {
    createCurrencyBlock(screen_main, start_x, section_y, currencyOne,
        &mainScreenCurrency1RateLabel, &mainScreenCurrency1FeeLabel, charge1);
    start_x += block_w + gap;
  }
  if (currencyTwo[0] != '\0') {
    createCurrencyBlock(screen_main, start_x, section_y, currencyTwo,
        &mainScreenCurrency2RateLabel, &mainScreenCurrency2FeeLabel, charge2);
    start_x += block_w + gap;
  }
  if (currencyThree[0] != '\0') {
    createCurrencyBlock(screen_main, start_x, section_y, currencyThree,
        &mainScreenCurrency3RateLabel, &mainScreenCurrency3FeeLabel, charge3);
  }

  if (currencyOne[0] != '\0')
    setCurrency(currencyOne, true);
}

/*** Display callback to flush the buffer to screen ***/
/**
 * @brief Flushes the display with the provided color data in the specified
 * area.
 *
 * This function is responsible for updating the display with the provided color
 * data in the specified area. It uses the startWrite(), setAddrWindow(),
 * pushPixels(), and endWrite() functions of the lcd object to perform the
 * display update.
 *
 * @param disp Pointer to the display driver structure.
 * @param area Pointer to the area structure specifying the region to update.
 * @param color_p Pointer to the color data array.
 */
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lcd.drawBitmap565(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);

  lv_disp_flush_ready(disp);
}

/*** Touchpad callback to read the touchpad ***/
void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  // The setup hotspot used to drop every tap, so RESTART on that screen could
  // never fire. Sample the controller slowly instead: a press still becomes a
  // click, and the portal is not doing an I2C read on every GUI tick.
  static unsigned long lastPortalTouchMs = 0;
  static lv_indev_state_t heldState = LV_INDEV_STATE_REL;
  static lv_point_t heldPoint = {0, 0};
  if (suspendTouchPolling) {
    const unsigned long now = millis();
    if (now - lastPortalTouchMs < 80) {
      data->state = heldState;
      data->point = heldPoint;
      return;
    }
    lastPortalTouchMs = now;
  }

  uint16_t touchX, touchY;
  bool touched = lcd.getTouch(&touchX, &touchY);

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;

    // Serial.printf("Touch (x,y): (%03d,%03d)\n",touchX,touchY );
  }

  if (suspendTouchPolling) {
    heldState = data->state;
    heldPoint = data->point;
  }
}

/**
 * Retrieves a Bolt invoice from a specified URL and stores it in the
 * 'boltInvoice' variable. The URL is assumed to be provided in the 'callback'
 * variable. If the request is successful, the invoice is extracted from the
 * JSON response and printed to the Serial monitor. If the invoice is not found
 * in the JSON response, an appropriate message is printed. If the HTTP GET
 * request fails, the function will retry after a delay of 3 seconds.
 */
/**
 * @brief Non-blocking function to check for Bolt invoice from callback URL.
 *
 * This function polls the callback URL to check if an invoice is available.
 * Returns true if invoice was found, false otherwise.
 *
 * @return true if invoice was successfully retrieved, false otherwise
 */
bool checkBoltInvoice() {
  return FundingService::pollBoltInvoice(sessionState);
}

/**
 * Sends a POST request to the GraphQL API endpoint with the provided Bolt
 * invoice. The request includes the necessary headers and payload to process
 * the payment.
 *
 * @param boltInvoice The Bolt invoice to be sent as part of the request
 * payload.
 */
bool getBlinkLnURL(const char *invoice) {
  int64_t invoicedSats = 0;
  if (!bolt11AmountSats(invoice, &invoicedSats) || invoicedSats != (int64_t)result) {
    Serial.print("Payout aborted: invoice sats ");
    Serial.print((long)invoicedSats);
    Serial.print(" != quoted ");
    Serial.println(result);
    char msg[80];
    snprintf(msg, sizeof(msg), "Invoice %ld sats, quote %ld",
             (long)invoicedSats, (long)result);
    FundingService::setPayoutFailure(msg);
    return false;
  }
  // Snapshot the wallet id under the background task's mutex. The task
  // publishes a new id when a balance fetch finishes.
  char walletId[128];
  const PriceBalanceWalletIdResult idResult =
      priceBalanceCopyWalletId(walletId, sizeof(walletId), 30000);
  if (idResult == PB_WALLETID_TASK_NOT_RUNNING) {
    // No concurrent writer exists; reading deviceState directly is safe.
    strlcpy(walletId, blinkwalletid, sizeof(walletId));
  } else if (idResult == PB_WALLETID_TIMEOUT) {
    // Writer may be mid-update — paying with a possibly torn id risks a
    // payout from the wrong wallet. Fail; the UI shows the error screen.
    Serial.println("Payout aborted: wallet id snapshot timed out");
    FundingService::setPayoutFailure("Blink wallet id was busy");
    return false;
  }
  if (FundingService::payInvoice(deviceState, invoice, walletId)) {
    return true;
  }
  if (FundingService::payoutFailureReason()[0] == '\0') {
    FundingService::setPayoutFailure("Blink did not pay the invoice");
  }
  return false;
}

/**
 * @brief Creates a LNURL withdrawal request and sends it to the specified API
 * endpoint.
 *
 * This function calculates the withdrawal amount in satoshis based on the total
 * amount and fiat value. If a charge percentage is specified, it deducts the
 * charge from the withdrawal amount. Then, it sends a POST request to the
 * primary API endpoint. If the request fails, it tries the secondary endpoint.
 * If the request is successful, it parses the response JSON and extracts the
 * LNURL and callback URL.
 *
 * @note This function requires the `http` library and the `primaryApiEndpoint`
 * and `secondaryApiEndpoint` variables to be defined.
 *
 * @return true when the proxy returned both the LNURL and the callback URL;
 *         false when no QR should be shown (caller must handle the failure).
 */
bool createLNURLWithdraw() {
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 | sessionState.totalCurrency3) != 0;
  if (!assignQuotedSats(mixed)) {
    return false;
  }
  Serial.print("Result (after fee, satoshis): ");
  Serial.println(result);

  return FundingService::requestLnurlWithdraw(sessionState, result);
}

/** Max insert limit for mixed mode, in EUR equivalent. */
static const float MAX_MIXED_EUR = 100.0f;

/** Get BTC/EUR rate (from whichever configured currency is EUR). */
static float getEurRateForLimit() {
  if ((strcmp(currencyOne, "EUR") == 0 || strcmp(currencyOne, "eur") == 0) && sessionState.fiatValue1 > 0)
    return sessionState.fiatValue1;
  if ((strcmp(currencyTwo, "EUR") == 0 || strcmp(currencyTwo, "eur") == 0) && sessionState.fiatValue2 > 0)
    return sessionState.fiatValue2;
  if ((strcmp(currencyThree, "EUR") == 0 || strcmp(currencyThree, "eur") == 0) && sessionState.fiatValue3 > 0)
    return sessionState.fiatValue3;
  return 0.0f;
}

/** Max satoshis for mixed mode (100 EUR equivalent). */
static long computeMixedMaxSats() {
  float eurRate = getEurRateForLimit();
  if (eurRate <= 0) return 999999999L;
  return (long)round(MAX_MIXED_EUR / eurRate * 1e8);
}

/** Total EUR-equivalent value of mixed amounts (before fee). */
static float computeMixedTotalValueEUR() {
  float eurRate = getEurRateForLimit();
  if (eurRate <= 0) return 0.0f;
  float sumEur = 0.0f;
  if (sessionState.totalCurrency1 > 0 && sessionState.fiatValue1 > 0)
    sumEur += (sessionState.totalCurrency1 / 100.0f) * (eurRate / sessionState.fiatValue1);
  if (sessionState.totalCurrency2 > 0 && sessionState.fiatValue2 > 0)
    sumEur += (sessionState.totalCurrency2 / 100.0f) * (eurRate / sessionState.fiatValue2);
  if (sessionState.totalCurrency3 > 0 && sessionState.fiatValue3 > 0)
    sumEur += (sessionState.totalCurrency3 / 100.0f) * (eurRate / sessionState.fiatValue3);
  return sumEur;
}

/**
 * @brief Quote the payout for the current session into `result`.
 * Mixed sessions quote every funded currency with its own rate and fee.
 * Returns false (and zeroes `result`) when the quote is refused.
 */
static bool assignQuotedSats(bool mixed) {
  Quote quote;
  if (mixed) {
    const MixedLeg legs[] = {
        {sessionState.totalCurrency1, sessionState.fiatValue1, charge1},
        {sessionState.totalCurrency2, sessionState.fiatValue2, charge2},
        {sessionState.totalCurrency3, sessionState.fiatValue3, charge3},
    };
    quote = quoteMixedSats(legs, 3);
  } else {
    quote = quoteSats(llround(total), fiatValue, chargeSelected);
  }
  if (!quote.ok || quote.sats > LONG_MAX) {
    result = 0;
    Serial.println("Quote refused: missing price, bad fee, or non-positive sats");
    return false;
  }
  result = (long)quote.sats;
  return true;
}

/**
 * @brief Retrieves the LNURL from the server based on the provided parameters.
 *
 * This function calculates the LNURL based on the total amount, EUR value, and
 * charge selected. It then sends a POST request to the server to retrieve the
 * LNURL. The LNURL is parsed from the response and stored in the lnURLgen
 * variable.
 *
 * @note This function assumes that the necessary variables (total, fiatValue,
 * chargeSelected, lnbitsURL, adminkey) have been properly initialized.
 */
bool getLNURL() {
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 | sessionState.totalCurrency3) != 0;
  if (!assignQuotedSats(mixed)) {
    return false;
  }
  Serial.print("Result (after fee, satoshis): ");
  Serial.println(result);

  if (lnbitsURL[0] == '\0') {
    Serial.println("Error: lnbitsURL is empty in getLNURL");
    return false;
  }

  http.end(); // Ensure previous connection is closed

  char requestUrl[512];
  snprintf(requestUrl, sizeof(requestUrl), "%s/withdraw/api/v1/links",
           lnbitsURL);
  if (!beginNetwork(http, secureClient, requestUrl)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", adminkey);

  const String resultStr = String(result);
  String httpRequestData = "{\"title\": \"Fiat Hell ";
  httpRequestData += "\", \"min_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"max_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"uses\": 1, \"wait_time\": 1, \"is_unique\": 1, "
                     "\"webhook_url\": \"\"}";
  const int httpCode = http.POST(httpRequestData);
  const String responsePayload = http.getString();
  http.end();

  if (httpCode != 200 && httpCode != 201) {
    Serial.print("getLNURL HTTP ");
    Serial.println(httpCode);
    lnURLgen[0] = '\0';
    return false;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, responsePayload)) {
    Serial.println("getLNURL JSON parse error");
    lnURLgen[0] = '\0';
    return false;
  }
  strlcpy(lnURLgen, doc["lnurl"] | "", sizeof(lnURLgen));
  if (lnURLgen[0] == '\0') {
    Serial.println("getLNURL response has no lnurl");
    return false;
  }
  strlcpy(sessionState.modifiedLnURLgen, lnURLgen,
          sizeof(sessionState.modifiedLnURLgen));
  return true;
}

/*LNbits offline*/
/**
 * Generates an LNURL for the ATM.
 *
 * This function generates an LNURL by performing the following steps:
 * 1. Generates a random 4-digit PIN.
 * 2. Generates a random 8-byte nonce.
 * 3. Encrypts the ATM secret using XOR encryption with the nonce and PIN.
 * 4. Encodes the encrypted payload in Base64 URL-safe format.
 * 5. Prepares the final LNURL by appending the encoded payload to the base URL.
 * 6. Converts the LNURL to a bech32-encoded string.
 * 7. Stores the bech32-encoded LNURL in the qrData variable.
 *
 * @note This function assumes that the following variables are defined:
 *       - secretATM: The secret key for the ATM.
 *       - baseURLATM: The base URL for the ATM.
 *       - total: The total amount for the transaction.
 *       - qrData: The variable to store the bech32-encoded LNURL.
 */
bool makeLNURL() {
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 |
                      sessionState.totalCurrency3) != 0;
  const OfflinePayout payout = offlineLnurlPayout(
      paymentService.isGaloy(deviceState.fundingSourceBuffer) ? FundingKind::Galoy
                                                             : FundingKind::Lnbits,
      mixed, llround(total), chargeSelected);
  if (!payout.allowed) {
    Serial.println("Offline LNURL refused");
    qrData[0] = '\0';
    return false;
  }

  int randomPin = random(1000, 9999);
  byte nonce[8];
  for (int i = 0; i < 8; i++) {
    nonce[i] = random(256);
  }

  byte payload[51];
  size_t payload_len = xor_encrypt(
      payload, sizeof(payload), (uint8_t *)secretATM, strlen(secretATM), nonce,
      sizeof(nonce), randomPin, payout.centsAfterFee);
  if (payload_len == 0) {
    Serial.println("Offline LNURL encrypt failed");
    return false;
  }
  String preparedURL = String(baseURLATM) + "?atm=1&p=";
  preparedURL +=
      toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
  Serial.println(preparedURL);

  // A truncated URL or LNURL still encodes as a QR the wallet cannot redeem.
  const size_t urlLen = preparedURL.length();
  // 8-bit to 5-bit regrouping needs ceil(urlLen * 8 / 5) values; bech32 adds
  // the "lnurl" prefix, the '1' separator, a 6-char checksum and the NUL.
  const size_t groups = (urlLen * 8 + 4) / 5;
  byte *data = (byte *)calloc(groups, sizeof(byte));
  char *charLnurl = (char *)calloc(groups + 5 + 1 + 6 + 1, sizeof(char));
  size_t len = 0;
  const bool encoded =
      data != nullptr && charLnurl != nullptr &&
      convert_bits(data, &len, 5, (const byte *)preparedURL.c_str(), urlLen, 8,
                   1) &&
      bech32_encode(charLnurl, "lnurl", data, len);
  bool fits = false;
  if (encoded) {
    to_upper(charLnurl);
    fits = strlcpy(qrData, charLnurl, sizeof(qrData)) < sizeof(qrData);
  }
  free(data);
  free(charLnurl);
  if (!fits) {
    Serial.println("Offline LNURL does not fit the QR buffer");
    qrData[0] = '\0';
    return false;
  }
  return true;
}

/**
 * Encrypts the given data using XOR encryption with a provided key and nonce.
 * The encrypted data is stored in the output buffer.
 *
 * @param output The buffer to store the encrypted data.
 * @param outlen The length of the output buffer.
 * @param key The encryption key.
 * @param keylen The length of the encryption key.
 * @param nonce The nonce used for encryption.
 * @param nonce_len The length of the nonce.
 * @param pin The PIN code to be encrypted.
 * @param amount_in_cents The amount to be encrypted.
 * @return The number of bytes written to the output buffer, or 0 if there was
 * not enough space.
 */
int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen,
                uint8_t *nonce, size_t nonce_len, uint64_t pin,
                uint64_t amount_in_cents) {
  // check we have space for all the data:
  // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
  if (outlen <
      2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8) {
    return 0;
  }

  int cur = 0;
  output[cur] = 1; // variant: XOR encryption
  cur++;

  // nonce_len | nonce
  output[cur] = nonce_len;
  cur++;
  memcpy(output + cur, nonce, nonce_len);
  cur += nonce_len;

  // payload, unxored first - <pin><currency byte><amount>
  int payload_len = lenVarInt(pin) + 1 + lenVarInt(amount_in_cents);
  output[cur] = (uint8_t)payload_len;
  cur++;
  uint8_t *payload = output + cur; // pointer to the start of the payload
  cur += writeVarInt(pin, output + cur, outlen - cur);             // pin code
  cur += writeVarInt(amount_in_cents, output + cur, outlen - cur); // amount
  cur++;

  // xor it with round key
  uint8_t hmacresult[32];
  SHA256 h;
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Round secret:", 13);
  h.write(nonce, nonce_len);
  h.endHMAC(hmacresult);
  for (int i = 0; i < payload_len; i++) {
    payload[i] = payload[i] ^ hmacresult[i];
  }

  // add hmac to authenticate
  h.beginHMAC(key, keylen);
  h.write((uint8_t *)"Data:", 5);
  h.write(output, cur);
  h.endHMAC(hmacresult);
  memcpy(output + cur, hmacresult, 8);
  cur += 8;

  // return number of bytes written to the output
  return cur;
}

void showMessageLVGL(String message) {
  // Create an LVGL label to display the message
  lv_obj_t *label = lv_label_create(screen_qr);
  lv_label_set_text(label, message.c_str());
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void showQRCodeLVGL(const char *data) {
  // Properly handle screen memory
  if (screen_qr != nullptr) {
    lv_obj_del(screen_qr); // Delete the previous screen if exists
  }

  screen_qr = lv_obj_create(NULL); // Create a new screen
  Serial.println("showQRCodeLVGL: Screen created");

  lv_color_t bg_color = lv_color_white();
  lv_color_t fg_color = lv_color_black();

  // Create the QR code
  lv_obj_t *qr = lv_qrcode_create(screen_qr, 200, fg_color, bg_color);
  if (qr == nullptr) {
    Serial.println("Failed to create QR code object.");
    return;
  }

  // Update QR code with the given data, ensuring data is valid
  if (data == nullptr || strlen(data) == 0 ||
      lv_qrcode_update(qr, data, strlen(data)) != LV_RES_OK) {
    Serial.println("Failed to update QR code.");
    return;
  }
  // Sit a little above center to leave room for the amounts below.
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, -30);

  // Add a border with bg_color
  lv_obj_set_style_border_color(qr, bg_color, 0);
  lv_obj_set_style_border_width(qr, 5, 0);

  // Create a label for the warning message
  lv_obj_t *labelWarning = lv_label_create(screen_qr);
  if (labelWarning == nullptr) {
    Serial.println("Failed to create labelWarning object.");
    return;
  }
  lv_label_set_text(labelWarning,
                    "IN CASE OF PROBLEMS, MAKE A PHOTO AND CONTACT SUPPORT");
  lv_obj_set_style_text_font(labelWarning, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(labelWarning, lv_color_hex(0xCCCCCC), 0);
  lv_obj_align(labelWarning, LV_ALIGN_BOTTOM_MID, 0, -5);

  // Create a label for the confirmation message
  lv_obj_t *label = lv_label_create(screen_qr);
  if (label == nullptr) {
    Serial.println("Failed to create label object.");
    return;
  }

  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    lv_label_set_text(label, "TAP ON SCREEN WHEN FINISHED");
  } else {
    lv_label_set_text(label, "SCAN AND WAIT FOR CONFIRMATION");
  }
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFF9900), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

  // What the customer gets and what they put in, under the code.
  char amountLine[96];
  if (result > 0) {
    snprintf(amountLine, sizeof(amountLine), "%ld sats", (long)result);
    lv_obj_t *satsLabel = lv_label_create(screen_qr);
    lv_label_set_text(satsLabel, amountLine);
    lv_obj_set_style_text_font(satsLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(satsLabel, lv_color_hex(0x90EE90), 0);
    lv_obj_align(satsLabel, LV_ALIGN_CENTER, 0, 130);
  }
  const bool mixedSale = (sessionState.totalCurrency1 | sessionState.totalCurrency2 |
                          sessionState.totalCurrency3) != 0;
  if (mixedSale) {
    const long cents[3] = {(long)sessionState.totalCurrency1,
                           (long)sessionState.totalCurrency2,
                           (long)sessionState.totalCurrency3};
    const char *names[3] = {currencyOne, currencyTwo, currencyThree};
    amountLine[0] = '\0';
    for (int i = 0; i < 3; i++) {
      if (cents[i] <= 0) {
        continue;
      }
      char part[40];
      snprintf(part, sizeof(part), "%s%ld %s", amountLine[0] ? " + " : "",
               cents[i] / 100, names[i]);
      strlcat(amountLine, part, sizeof(amountLine));
    }
    strlcat(amountLine, " inserted", sizeof(amountLine));
  } else {
    snprintf(amountLine, sizeof(amountLine), "%ld %s inserted",
             (long)llround((double)total / 100.0), currencySelected);
  }
  lv_obj_t *fiatLabel = lv_label_create(screen_qr);
  lv_label_set_text(fiatLabel, amountLine);
  lv_obj_set_style_text_font(fiatLabel, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(fiatLabel, lv_color_hex(0xC8C8C8), 0);
  lv_obj_align(fiatLabel, LV_ALIGN_CENTER, 0, 172);

  // QR and labels are set above; load the screen once so the panel draws once.
  lv_scr_load(screen_qr);
  attachBatteryToCurrentScreen();

  // Debugging heap memory
  Serial.print("Free heap (showQRCodeLVGL): ");
  Serial.println(ESP.getFreeHeap());

  printHeapStatus(); // Print heap status for debugging
}

void printHeapStatus() {
  // getHeapSize() / getMaxAllocHeap() volajú heap_caps_get_info(), ktorý
  // iteruje cez všetky bloky pod zámkom s vypnutými interruptmi. Pri
  // súčasnom TCP traffiku (Blink HTTPS) to spôsobilo Interrupt WDT
  // timeout a deadlock — preto len rýchly counter read.
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
}

/*void btn_reset_event_handler(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    Serial.println("Back button pressed - restarting device");
    // Restart device instead of going back to main screen
    // This prevents issues where user inserts money and immediately presses
    // Back, which could cause incorrect currency handling
    ESP.restart();
  }
}*/

/**
 * @brief Starts the configuration portal.
 *
 * This function is responsible for starting the configuration portal, which
 * allows the user to configure the device settings. It assumes that the
 * 'config' and 'portal' objects have been previously defined and configured
 * appropriately.
 *
 * @note This function enters an infinite loop until the configuration process
 * is completed.
 */
void startConfigPortal() {
  Serial.println("Entered Config Portal");

  // Assume config and portal are previously defined and configured
  // appropriately
  acConfig.immediateStart = true;
  portal.join({elementsAux, saveAux, firstAux, savefirstAux, secondAux,
               savesecondAux, thirdAux, savethirdAux, guiAux, saveguiAux,
               otaAux, otaDoAux});
  portal.config(acConfig);
  portal.begin();
  Serial.println("Portal started. IP2: " + WiFi.localIP().toString());
  // No infinite loop; portal.handleClient() is called in the main loop
  // timer = 2000;
}

/* Back button */

// Function to create a back button

/*void createResetButton(lv_obj_t *parent)

{
  if (parent == NULL)
    return; // Safety check

  // Delete existing back button if it exists
  if (btn_reset != nullptr) {
    lv_obj_del(btn_reset);
    btn_reset = nullptr;
  }

  btn_reset =
      lv_btn_create(parent); // Create button on the provided parent object
  lv_obj_set_size(btn_reset, 80, 40);
  lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_t *btn_label_back = lv_label_create(btn_reset);
  lv_label_set_text(btn_label_back, "Restart");
  lv_obj_center(btn_label_back);
  lv_obj_add_event_cb(btn_reset, btn_reset_event_handler, LV_EVENT_CLICKED, NULL);
}*/

/**
 * @brief Flag indicating whether the loop is currently reading.
 *
 * This flag is used to control the execution flow in the main loop.
 * When set to true, it indicates that the loop is currently reading data.
 * When set to false, it indicates that the loop is not reading data.
 *
 * @note This flag should be accessed and modified in a thread-safe manner
 *       when used in a multi-threaded environment.
 */
volatile bool isLoopReading = false;

/**
 * @brief Handles UI state machine transitions non-blockingly.
 *
 * This function processes state transitions based on current state, timestamps,
 * and user input. It replaces blocking while/delay loops with non-blocking
 * state checks.
 */
// Armed by a 3-second BOOT press on the main screen. Closed again from loop().
static unsigned long configModeActiveUntil = 0;

void triggerRuntimeConfigMode() {
  if (configModeActiveUntil) return; // already active
  portalRequestedByUser = true;
  // Say something first: switching the radio to hotspot mode takes a second
  // or two, and a press with no reaction looks ignored.
  showPayoutWaitScreen("Starting setup hotspot...");
  // No notes while the operator is in setup, and the price refresh must not
  // reopen the acceptor behind this screen.
  billAcceptorWrite(185);
  acceptorArmed = false;
  mainScreenShown = false;
  // Hotspot alone, same as a boot-time setup session. AP+STA would publish
  // two addresses for fiathell.local and the page would stall again.
  settleSetupHotspot();
  configModeActiveUntil = millis() + 5UL * 60UL * 1000UL;
  Serial.println("Config mode active: " + acConfig.apid + " -> 192.168.4.1  (5 min)");
  createPortalScreen();
  discardPayoutWaitScreen();
}

// Setup session over: put the customer screen back, reopen the acceptor
// through the normal price check, and drop the setup screen.
static void closeRuntimeConfigScreen() {
  if (screen_portal == nullptr) {
    return;
  }
  if (lv_scr_act() == screen_portal) {
    if (screen_main == nullptr) {
      return;
    }
    mainScreenShown = true;
    updateMainScreenLabel();
    lv_scr_load(screen_main);
    attachBatteryToCurrentScreen();
  }
  if (lv_scr_act() != screen_portal) {
    lv_obj_del(screen_portal);
    screen_portal = nullptr;
  }
}

// Drop the finished sale and show the screen the next customer already
// expects. Wi-Fi, prices, and the startup check stay up; a reboot here made
// every payout wait through the whole boot again.
static void returnToMainScreen() {
  sessionState.resetTransaction();
  sessionState.resetPaymentFlow();
  qrDebounceDone = false;
  mixedLimitExceededAutoProceed = false;
  FundingService::clearPayoutFailure();
  // The payout inhibited the acceptor while this flag still said it was open.
  // Mark idle before the screen update so that update reopens the acceptor;
  // any earlier state (QR, paying, thank-you, error) keeps it shut.
  acceptorArmed = false;
  billAcceptorWrite(185);
  currentUiState = UI_IDLE;
  if (screen_main == nullptr) {
    createMainScreen();
  } else {
    // The balance dropped by the paid amount; show the cached value now and
    // let the background task refresh it without blocking the next customer.
    triggerPriceBalanceFetch(PBR_PERIODIC);
    mainScreenShown = true;
    updateMainScreenLabel();
    lv_scr_load(screen_main);
    attachBatteryToCurrentScreen();
  }
  uiController.deleteThankYouScreen();
  uiController.deleteQRCodeScreen();
  discardInsertMoneyScreen();
  if (screen_payment_error != nullptr && lv_scr_act() != screen_payment_error) {
    lv_obj_del(screen_payment_error);
    screen_payment_error = nullptr;
  }
  stateEnterTime = millis();
  Serial.println("Transaction cleared => main screen");
}

void handleUiStateMachine() {
  unsigned long currentTime = millis();
  BTNA.read();

  // Long-press BOOT on the main screen opens the setup hotspot for a few
  // minutes, so the operator does not have to reboot to change settings.
  if (currentUiState == UI_IDLE && appStartupCompleted) {
    static unsigned long bootPressStart = 0;
    if (BTNA.isPressed()) {
      if (bootPressStart == 0) {
        bootPressStart = currentTime;
      } else if (currentTime - bootPressStart >= 3000) {
        bootPressStart = 0;
        Serial.println("BOOT long-press => config hotspot");
        triggerRuntimeConfigMode();
      }
    } else {
      bootPressStart = 0;
    }
  }

  switch (currentUiState) {
  case UI_LOGO_WAIT: {
    // Wait for 5 seconds or tap during logo screen
    // Check both hardware button (BTNA) and touchscreen
    bool tapDetected = false;

    // Check hardware button first
    if (BTNA.wasPressed()) {
      tapDetected = true;
      Serial.println("Logo tap detected (BTNA) => triggerAp = true");
    }

    // Also check touchscreen directly (for display touch)
    uint16_t touchX, touchY;
    if (lcd.getTouch(&touchX, &touchY)) {
      tapDetected = true;
      Serial.print("Logo tap detected (touchscreen) at (");
      Serial.print(touchX);
      Serial.print(",");
      Serial.print(touchY);
      Serial.println(") => triggerAp = true");
    }

    if (tapDetected) {
      triggerAp = true;
      showLogoSettingsProgress();
      currentUiState = UI_IDLE;
    } else if (currentTime - stateEnterTime >= 2000) {
      currentUiState = UI_IDLE;
      Serial.println("Logo wait timeout => proceeding");
    }
    break;
  }

  case UI_INSERTING_MONEY:
    // State is set when bill is detected, handled in main loop
    // Transition to SHOWING_QR happens when total reached (handled elsewhere)
    break;

  case UI_SHOWING_QR:
    // After QR is shown, wait for debounce (1000ms) then transition
    if (currentTime - stateEnterTime >= 1000) {
      if (!qrDebounceDone) {
        qrDebounceDone = true;
        Serial.println("QR debounce done");
      }
      // For Blink, transition to waiting for invoice from proxy server
      // For LNbits, transition to waiting for tap
      if (isBlinkFlow) {
        currentUiState = UI_WAITING_FOR_BLINK_INVOICE;
        stateEnterTime = currentTime;
        lastBlinkPollTime = 0; // Reset poll timer
        Serial.println("State: SHOWING_QR -> WAITING_FOR_BLINK_INVOICE");
      } else {
        currentUiState = UI_WAITING_FOR_TAP;
        stateEnterTime = currentTime;
        Serial.println("State: SHOWING_QR -> WAITING_FOR_TAP");
      }
    }
    break;

  case UI_WAITING_FOR_BLINK_INVOICE:
    // Non-blocking polling for Blink invoice from callback URL
    // Poll every 2 seconds
    if (currentTime - lastBlinkPollTime >= 2000) {
      lastBlinkPollTime = currentTime;
      Serial.println("Polling for Blink invoice...");

      if (checkBoltInvoice()) {
        // Invoice received! Pay it and show the matching result screen
        Serial.println("Invoice received => processing payment");
        showPayoutWaitScreen();
        bool paymentOk = runBlinkPayment();
        uiController.deleteQRCodeScreen();
        if (paymentOk) {
          createThankYouScreen();
          currentUiState = UI_THANK_YOU;
          stateEnterTime = millis();
          isBlinkFlow = false;
        } else {
          createPaymentErrorScreen();
          currentUiState = UI_PAYMENT_ERROR;
          stateEnterTime = millis();
          isBlinkFlow = false;
        }
        lv_task_handler();
        discardPayoutWaitScreen();
        // currentTime was taken before the payment request. The result screen
        // stamps stateEnterTime with a later millis(), and the unsigned
        // subtraction below would then look like the 5-minute timeout and
        // replace a successful payout with PAYMENT FAILED.
        break;
      } else if (FundingService::payoutIsFatal()) {
        Serial.println("Blink invoice unusable => payment error");
        uiController.deleteQRCodeScreen();
        enterPaymentError();
        isBlinkFlow = false;
        break;
      }
      // If no invoice yet, continue polling (will check again in 2 seconds)
    }
    // Still waiting for an invoice. Once a result screen is up, this case
    // has already returned.
    if (currentTime - stateEnterTime >= 300000) { // 5 minutes timeout
      char msg[160];
      const char *note = FundingService::lastPollNote();
      snprintf(msg, sizeof(msg), "No invoice from wallet (%.90s)",
               (note && note[0]) ? note : "no reply");
      FundingService::setPayoutFailure(msg);
      Serial.println("Blink invoice timeout => payment error");
      uiController.deleteQRCodeScreen();
      enterPaymentError();
      isBlinkFlow = false;
    }
    break;

  case UI_WAITING_FOR_TAP: {
    // Non-blocking wait for tap after QR code (for LNbits)
    uint16_t qrTouchX, qrTouchY;
    if (BTNA.wasPressed() || lcd.getTouch(&qrTouchX, &qrTouchY)) {
      // Reset for the next transaction
      Serial.println("Tap detected => back to main screen");
      returnToMainScreen();
    }
    break;
  }

  case UI_THANK_YOU: {
    // Thank you screen: 5 seconds or tap to continue
    uint16_t thxTouchX, thxTouchY;
    bool thxTap = lcd.getTouch(&thxTouchX, &thxTouchY) || BTNA.wasPressed();
    if (thxTap || (currentTime - stateEnterTime >= 5000)) {
      Serial.println("Thank you => back to main screen");
      returnToMainScreen();
    }
    break;
  }

  case UI_PAYMENT_ERROR:
    // Keep the error visible long enough to be read/photographed, then go
    // back to the main screen. The reason stays in /payout-error.txt; a
    // reboot here cost the next customer a full boot.
    if (currentTime - stateEnterTime >= 30000) {
      Serial.println("Payment error timeout => main screen");
      isBlinkFlow = false;
      returnToMainScreen();
    }
    break;

  case UI_IDLE:
  default:
    // Idle state - no special handling needed
    break;
  }
}

/**
 * @brief The main loop function that runs repeatedly in the program.
 *
 * This function is responsible for handling the GUI, checking the balance,
 * updating the main screen label, detecting the insertion of money, processing
 * the total, and waiting for user input to go back to the main screen. It also
 * includes a delay of 5 milliseconds at the end of each iteration.
 */
void loop() {
  if (pendingPreflight) {
    pendingPreflight = false;
    preflightGate();
    finishStartupAfterPreflight();
  }

  // Deferred restart (after /setup/save or /setup/ota response has been sent)
  if (pendingRestartAt && millis() > pendingRestartAt) {
    pendingRestartAt = 0;
    ESP.restart();
  }

  // Auto-close config mode AP after 5-minute timeout (defensive — only ever
  // triggered if triggerRuntimeConfigMode() was called by some future caller).
  if (configModeActiveUntil && millis() > configModeActiveUntil) {
    configModeActiveUntil   = 0;
    portalRequestedByUser   = false;
    acConfig.preserveAPMode = false;
    acConfig.autoReconnect  = true;
    portal.config(acConfig);
    stopApDns();
    if (WiFi.getMode() & WIFI_AP) WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    if (wifiStatus()) startFiathellMdns();
    Serial.println("Config mode timed out — AP closed");
    closeRuntimeConfigScreen();
  }

  lv_timer_handler();    // Let the GUI do its work

  // Own captive-portal DNS: claim port 53 before AutoConnect's handleClient so
  // its callback keeps it from starting a second resolver. Keep it running
  // while the AP is up (so the sign-in prompt appears on phones), stop it once
  // the AP is gone.
  if (WiFi.getMode() & WIFI_AP) {
    ensureApDns();
    if (apDnsRunning) processApDns();
  } else {
    stopApDns();
  }

  // Not truly non-blocking: a response write waits up to 10 s per chunk when
  // the client stops ACKing (WiFiClient::write retries), freezing loop().
  const unsigned long handleStart = millis();
  portal.handleClient();
  const unsigned long handleMs = millis() - handleStart;
  if (handleMs > 500) {
    Serial.printf("[HTTP] loop blocked %lu ms in handleClient (last uri %s)\n",
                  handleMs, server.uri().c_str());
  }

  if (pendingConfigReload) {
    reloadRuntimeConfigFromFlash();
  }

  const bool portalActive = configPortalUp();
  suspendTouchPolling = portalActive;
  if (portalActive) {
    logPortalHeartbeat();
  }

  if (pendingPortalCompletion && wifiStatus()) {
    if (portalRequestedByUser || portalRequiredForMissingConfig) {
      if (!portalNetworkStateLogged) {
        if (!(WiFi.getMode() & WIFI_AP)) {
          Serial.println("Re-enabling config AP alongside STA");
          WiFi.mode(WIFI_AP_STA);
          WiFi.softAP(acConfig.apid.c_str(), acConfig.psk.c_str());
          ensureApDns();
        }

        Serial.println("WiFi connected in config portal; staying in settings mode");
        Serial.println("Portal available on home WiFi IP: " +
                       WiFi.localIP().toString());
        Serial.println("Portal AP IP: " + WiFi.softAPIP().toString());
        portalNetworkStateLogged = true;
      }
    } else {
      Serial.println("WiFi connected from portal flow; completing startup");
      completeStartupAfterPortal();
    }
  }

  // Keep the portal responsive and avoid unrelated app work while a client is
  // still configuring WiFi through AutoConnect.
  if (portalActive && !wifiStatus()) {
    delay(5);
    return;
  }
  // Handle UI state machine
  handleUiStateMachine();

  // Process background fetch results (periodic price/balance update)
  if (consumePriceBalanceDataReady()) {
    updateMainScreenLabel();
    if (currentUiState == UI_INSERTING_MONEY) {
      refreshInsertMaxLabel();
    }
    lv_task_handler();
  }

  if (initialCheck) {
    // Start the refresh cadence from now instead of firing immediately - the
    // startup check already fetched a fresh price and balance.
    previousMillis = millis();
    initialCheck = false;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    checkNetworkAndDeviceStatus();
    triggerPriceBalanceFetch(PBR_PERIODIC);
  }

  if (battery_container != nullptr &&
      currentMillis - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdate = currentMillis;
    updateBatteryIndicator();
  }

  // Check if user is inserting money
  int x = nonBlockingRead();

#if BILL_ACCEPTOR_ENABLED
  if (x != -1) {
    Serial.print("NV10 rx: ");
    Serial.print(x);
    if (x >= 1 && x <= (int)billAmountIntOne.size()) {
      Serial.println(" (channel)");
    } else {
      Serial.println(" (other)");
    }
  }
#endif

  if (x >= 1 && x <= (int)billAmountIntOne.size()) {
    if (currentUiState != UI_IDLE && currentUiState != UI_INSERTING_MONEY) {
      billAcceptorWrite(185);
    } else {
    int channelIdx = x - 1;  // 0-based
    int amount = billAmountIntOne[channelIdx];
    const bool mixed = mixedCurrenciesConfigured();

    if (creditBillTotals(channelIdx)) {
      if (!isInsertingMoney) {
        createInsertMoneyScreen();
        lv_task_handler();
        isInsertingMoney = true;
        currentUiState = UI_INSERTING_MONEY;
        stateEnterTime = millis();
      }
      if (mixed) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Last bill: %d %s", amount, sessionState.lastBillCurrency);
        lv_label_set_text(labelLastInserted, buf);
        // Same quote functions as the payout, so the screen shows what is paid.
        const MixedLeg legs[] = {
            {sessionState.totalCurrency1, sessionState.fiatValue1, charge1},
            {sessionState.totalCurrency2, sessionState.fiatValue2, charge2},
            {sessionState.totalCurrency3, sessionState.fiatValue3, charge3},
        };
        lv_obj_t *const legLabels[] = {labelTotalCurrency1, labelTotalCurrency2,
                                       labelTotalCurrency3};
        const char *const legCurrencies[] = {currencyOne, currencyTwo, currencyThree};
        for (int leg = 0; leg < 3; leg++) {
          if (legLabels[leg] == nullptr) {
            continue;
          }
          if (legs[leg].cents <= 0) {
            lv_label_set_text(legLabels[leg], "");
            continue;
          }
          const Quote legQuote =
              quoteSats(legs[leg].cents, legs[leg].btcPrice, legs[leg].feePercent);
          snprintf(buf, sizeof(buf), "Total %s: %.2f %s (%lld sats)",
                   legCurrencies[leg], legs[leg].cents / 100.0,
                   legCurrencies[leg], legQuote.ok ? (long long)legQuote.sats : 0LL);
          lv_label_set_text(legLabels[leg], buf);
        }
        if (labelTotalSats) {
          const Quote totalQuote = quoteMixedSats(legs, 3);
          snprintf(buf, sizeof(buf), "Total: %lld sats",
                   totalQuote.ok ? (long long)totalQuote.sats : 0LL);
          lv_label_set_text(labelTotalSats, buf);
        }
        lv_label_set_text(labelTotalAmount, "");
        if (labelMaxAmount && getEurRateForLimit() <= 0.0f) {
          // No EUR among the configured currencies, so the 100 EUR cap does
          // not apply (it used to print "Max: 100 EUR (~999999999 sats)").
          // What limits the sale is the wallet: show what it can still pay.
          int64_t walletSats = 0;
          const Quote soFar = quoteMixedSats(legs, 3);
          if (currentWalletSats(&walletSats)) {
            int64_t left = walletSats - (soFar.ok ? soFar.sats : 0);
            if (left < 0) left = 0;
            snprintf(buf, sizeof(buf), "Max: ~%lld sats left", (long long)left);
          } else {
            buf[0] = '\0';
          }
          setLabelTextIfChanged(labelMaxAmount, buf);
        } else if (labelMaxAmount) {
          long maxSats = computeMixedMaxSats();
          float totalEUR = computeMixedTotalValueEUR();
          if (totalEUR >= MAX_MIXED_EUR) {
            snprintf(buf, sizeof(buf), "Max reached - generating QR...");
#if BILL_ACCEPTOR_ENABLED
            billAcceptorWrite(185);
#endif
            mixedLimitExceededAutoProceed = true;
          } else {
            snprintf(buf, sizeof(buf), "Max: 100 EUR (~%ld sats)", maxSats);
          }
          lv_label_set_text(labelMaxAmount, buf);
        }
      } else {
        refreshInsertSummary(amount);
        if (labelTotalCurrency1) lv_label_set_text(labelTotalCurrency1, "");
        if (labelTotalCurrency2) lv_label_set_text(labelTotalCurrency2, "");
        if (labelTotalCurrency3) lv_label_set_text(labelTotalCurrency3, "");
      }
    }
    }
  }
  // Check button release, touchscreen tap, or total (only if in INSERTING_MONEY state)
  const bool hasMixed = (sessionState.totalCurrency1 || sessionState.totalCurrency2 || sessionState.totalCurrency3) != 0;
  const bool hasAmount = (total != 0) || hasMixed;
  if (currentUiState == UI_INSERTING_MONEY) {
    // Keep each channel in step with the offset balance. A note larger than
    // what is left is inhibited; smaller notes stay open until none fit.
    if (!mixedCurrenciesConfigured()) {
      refreshInsertMaxLabel();
      long cap = 0;
      const long left = insertRoomLeft(&cap);
      applyFittingNotes(cap > 0 ? left : 0);
    } else if (!offlineLnbitsMode()) {
      // Several currencies: the wallet limit is counted in sats, including
      // the cash already inserted.
      int64_t walletSats = 0;
      const uint16_t mask = currentWalletSats(&walletSats)
                                ? mixedNoteMask(walletSats)
                                : 0;
      applyNoteMask(mask, (long)walletSats, "sats");
      if (mask == 0 && labelMaxAmount != nullptr && !mixedLimitExceededAutoProceed) {
        setLabelTextIfChanged(labelMaxAmount, "Max reached — press FINISH");
      }
    }

    // maxamountSelected of 0 is an empty "max withdraw" field, not a limit of
    // zero. The wallet gate above is what stops a note the balance cannot pay.
    const bool hitConfiguredMax =
        !hasMixed && maxamountSelected > 0.0f && total >= maxamountSelected;
    const bool finishPressed = insertFinishTapped;
    insertFinishTapped = false;
    if ((BTNA.wasPressed() && hasAmount) || (finishPressed && hasAmount) ||
        mixedLimitExceededAutoProceed || hitConfiguredMax) {
      mixedLimitExceededAutoProceed = false;
      // Acknowledge FINISH at once: spinner up, insert screen gone. The
      // settle wait and the payout request below both take seconds.
      result = 0;
      showPayoutWaitScreen();
      discardInsertMoneyScreen();
      inhibitAndCollectPendingBills();
      const bool payMixed = (sessionState.totalCurrency1 || sessionState.totalCurrency2 ||
                             sessionState.totalCurrency3) != 0;
      if (payMixed) {
        // Mixed-currency: use first wallet for LNURL (undef macros to use struct members)
#if defined(baseURLATM) && defined(secretATM) && defined(lnbitsURL)
#undef baseURLATM
#undef secretATM
#undef lnbitsURL
#endif
        if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
          strlcpy(sessionStatePtr->baseURLATM, baseURLATM1, sizeof(sessionStatePtr->baseURLATM));
          strlcpy(sessionStatePtr->secretATM, secretATM1, sizeof(sessionStatePtr->secretATM));
          int slashCount = 0, thirdSlash = 0;
          for (size_t i = 0; baseURLATM1[i] != '\0'; i++) {
            if (baseURLATM1[i] == '/') {
              slashCount++;
              if (slashCount == 3) {
                thirdSlash = (int)i;
                break;
              }
            }
          }
          if (thirdSlash > 0 && thirdSlash < (int)sizeof(deviceStatePtr->lnbitsURL)) {
            memcpy(deviceStatePtr->lnbitsURL, baseURLATM1, (size_t)thirdSlash);
            deviceStatePtr->lnbitsURL[thirdSlash] = '\0';
          } else {
            strlcpy(deviceStatePtr->lnbitsURL, baseURLATM1, sizeof(deviceStatePtr->lnbitsURL));
          }
        }
#define baseURLATM sessionState.baseURLATM
#define secretATM sessionState.secretATM
#define lnbitsURL deviceState.lnbitsURL
        Serial.println(F("Mixed-currency: computing total sats from all currencies"));
      } else {
        total = (coins + bills) * 100;
        Serial.print(F("Total: "));
        Serial.println(total);
      }

      PayoutJob job = {false, false, nullptr};
      runPayoutRequest(&job);
      const char *qrPayload = job.payload;
      const bool blinkFlow = job.blinkFlow;
      if (qrPayload == nullptr) {
        if (FundingService::payoutFailureReason()[0] == '\0') {
          FundingService::setPayoutFailure("Could not create the withdraw QR");
        }
        Serial.println("Payout not created => payment error screen");
        enterPaymentError();
      } else {
        showQRCodeLVGL(qrPayload);
        lv_task_handler();
        currentUiState = UI_SHOWING_QR;
        stateEnterTime = millis();
        qrDebounceDone = false;
        isBlinkFlow = blinkFlow;
      }
      discardPayoutWaitScreen();
    }

    lv_task_handler(); // Call LVGL task handler
    yield();           // Yield to other tasks (non-blocking, prevents watchdog)
  }
}
