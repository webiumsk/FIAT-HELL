//= == == == == == == == == == == == == == == == == == == == == == == == == ==
//== == = //
//============EDIT IF USING DIFFERENT HARDWARE============//
//========================================================//
// v1.2.1
#define FW_VERSION "1.2.5"

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

lv_color_t colors[] = {LV_COLOR_PURPLE, LV_COLOR_RED,   LV_COLOR_ORANGE,
                       LV_COLOR_YELLOW, LV_COLOR_GREEN, LV_COLOR_BLUE};

#include <FS.h>
#include <esp_system.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

using WebServerClass = WebServer;
fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true

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
#include <HTTPUpdate.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "PriceBalanceTask.h"
#define AUTOCONNECT_USE_LOG 1
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <JC_Button.h>
#include <SPI.h>

#include <ESPmDNS.h>
#include <Bitcoin.h>
#include <HTTPClient.h>
#include <Hash.h>

#include <iostream>
#include <vector>

#include <cstring> // For memset
char Buf[200];     // Buffer for the encrypted data

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
#include "services/PaymentService.h"
#include "services/UiController.h"
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
#define tempCharge sessionState.tempCharge
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

// Temporary buffers
char buffer[32];

const long interval = 300000; // 5 minutes in milliseconds

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

const char *graphqlEndpoint = "https://api.blink.sv/graphql";
const char *primaryApiEndpoint = "https://api.lnbc.sk/v1/lnurl";
const char *secondaryApiEndpoint = "https://api.lnurlproxy.me/v1/lnurl";
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

WiFiClientSecure *secureClientPtr = nullptr;
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
  int raw = analogRead(BATTERY_ADC_GPIO);
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
  lv_obj_clear_flag(battery_container, LV_OBJ_FLAG_HIDDEN);
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
  int filled;
  lv_color_t color;
  if (pct < 0) {
    filled = 0;
    color = lv_color_hex(0x808080);
    lv_label_set_text(battery_label, "---");
  } else {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(battery_label, buf);
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
#include "pagesetup.h"

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
void createMainScreen();
void createInsertMoneyScreen();
void createSwitch(lv_obj_t *parent);
void createAcceptedCurrenciesSection();
void updateBurnText();
void updateMainScreenLabel();
void checkPrice();
void checkPriceCoinGecko();
void checkPriceExchangeApi();
void checkBalance();
bool isLNbits();
bool wifiStatus();
void showQRCodeLVGL(const char *data);
int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen,
                uint8_t *nonce, size_t nonce_len, uint64_t pin,
                uint64_t amount_in_cents);
static long computeMixedTotalSats();
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

static size_t billAcceptorWrite(uint8_t value) {
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

static bool exitCaptivePortalLoopOnce = false;
static bool suspendTouchPolling = false;
static bool pendingPortalCompletion = false;
static bool appStartupCompleted = false;
static bool portalRequestedByUser = false;
static bool portalRequiredForMissingConfig = false;
static bool portalRequiredForWifiRecovery = false;
static bool portalNetworkStateLogged = false;
static bool pendingConfigReload = false;
static unsigned long pendingRestartAt = 0;

static bool allowSetupToContinueWhilePortalStaysAlive() {
  if (exitCaptivePortalLoopOnce) {
    exitCaptivePortalLoopOnce = false;
    Serial.println("Leaving blocking captive portal loop; keeping portal alive");
    return false;
  }
  return true;
}

void completeStartupAfterPortal() {
  if (appStartupCompleted) {
    return;
  }

  if (wifiStatus() && MDNS.begin("fiathell")) {
    Serial.println("mDNS: http://fiathell.local");
  }

  Serial.println("Proceeding to main screen");
  createMainScreen();
  lv_task_handler();
  bootStage(50, "main screen created");

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
  Serial.print("ESP Free heap (Setup end): ");
  Serial.println(ESP.getFreeHeap());

  startPriceBalanceTask(deviceStatePtr, sessionStatePtr);
  triggerPriceBalanceFetch(PBR_PERIODIC);

  appStartupCompleted = true;
  pendingPortalCompletion = false;
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

  acConfig.psk = deviceState.password;
  acConfig.password = deviceState.password;
  portal.config(acConfig);

  if (appStartupCompleted) {
    uiController.deleteMainScreen();
    createMainScreen();
    triggerPriceBalanceFetch(PBR_PERIODIC);
    updateMainScreenLabel();
    lv_task_handler();
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
  bootStage(1, "serial ready");

  bootStage(2, "allocating runtime objects");
  deviceStatePtr = new DeviceState();
  sessionStatePtr = new SessionState();
  contentPtr = new String();
  serverPtr = new WebServerClass();
  portalPtr = new AutoConnect(server);
  secureClientPtr = new WiFiClientSecure();
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
  if (TX2 >= 0) {
    SerialPort2.begin(4800, SERIAL_8N1, -1, TX2); // Coin mech
  }
  if (INHIBITMECH >= 0) {
    pinMode(INHIBITMECH, OUTPUT);
  }
  bootStage(14, "serial peripherals initialized");

  secureClient.setInsecure();
  bootStage(15, "secure client configured");

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

  // Serial config upload window: web installer sends WRITE_CONFIG:/file.json:{json}
  // then CONFIG_DONE. Device writes files to SPIFFS and continues normal boot.
  // Only blocks if data arrives within 300 ms — normal boots are not delayed.
  Serial.println("FIAT-HELL:CONFIG_READY");
  {
    const unsigned long detectEnd = millis() + 2000UL;
    while (millis() < detectEnd && !Serial.available()) yield();
    if (Serial.available()) {
      const unsigned long cfgDeadline = millis() + 10000UL;
      while (millis() < cfgDeadline) {
        if (Serial.available()) {
          String line = Serial.readStringUntil('\n');
          line.trim();
          if (line.startsWith("WRITE_CONFIG:")) {
            // format: WRITE_CONFIG:/filename.json:{json_content}
            const int colon2 = line.indexOf(':', 13);
            if (colon2 > 13) {
              String filePath    = line.substring(13, colon2);
              String fileContent = line.substring(colon2 + 1);
              File f = SPIFFS.open(filePath, "w");
              if (f) { f.print(fileContent); f.close(); }
              Serial.print("FIAT-HELL:WROTE:");
              Serial.println(filePath);
            }
          } else if (line == "CONFIG_DONE") {
            Serial.println("FIAT-HELL:CONFIG_SAVED");
            break;
          }
        }
        yield();
      }
    }
  }

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

  // Returns true only when the request comes from a client on the AP subnet.
  // Blocks portal access from the STA (public WiFi) interface.
  auto isApClient = []() -> bool {
    const IPAddress c = server.client().remoteIP();
    return c[0] == 192 && c[1] == 168 && c[2] == 4;
  };

  server.on("/", [isApClient]() {
    const bool routeToConfigPortal =
        pendingPortalCompletion || portalRequestedByUser ||
        portalRequiredForMissingConfig || portal.isPortalAvailable();
    if (routeToConfigPortal) {
      if (!isApClient()) {
        server.send(403, "text/plain",
          "Portal accessible only via AP — hold BOOT button 3 s to enable");
        server.client().stop();
        return;
      }
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "");
      server.client().stop();
      return;
    }

    const String page = content + AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", page);
  });
  bootStage(22, "root route registered");

  auto redirectToConfigPortal = [isApClient]() {
    if (!isApClient()) {
      server.send(403, "text/plain",
        "Portal accessible only via AP — hold BOOT button 3 s to enable");
      server.client().stop();
      return;
    }
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    server.client().stop();
  };

  // Common captive-portal probe URLs used by Android, iPhone, and Windows.
  // Android: /generate_204 — return 204 when STA (no notification), 302 → /setup when AP.
  server.on("/generate_204", [isApClient]() {
    if (!isApClient()) { server.send(204, "text/plain", ""); return; }
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    server.client().stop();
  });
  // iOS/macOS: /hotspot-detect.html — return "Success" when STA, 302 → /setup when AP.
  server.on("/hotspot-detect.html", [isApClient]() {
    if (!isApClient()) {
      server.send(200, "text/html",
        "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
      return;
    }
    server.sendHeader("Location", "/setup", true);
    server.send(302, "text/plain", "");
    server.client().stop();
  });
  server.on("/connecttest.txt", redirectToConfigPortal);
  server.on("/ncsi.txt", redirectToConfigPortal);
  server.on("/fwlink", redirectToConfigPortal);
  server.on("/favicon.ico", []() { server.send(204, "text/plain", ""); });

  // Lightweight mobile config portal — served only to AP clients (192.168.4.x).
  server.on("/setup", HTTP_GET, [isApClient]() {
    if (!isApClient()) {
      server.send(403, "text/plain", "Setup only via AP — hold BOOT 3 s to enable");
      return;
    }
    // Use deviceStatePtr-> for non-macro fields; bare macros for aliased fields.
    const bool isLNbitsMode = (strcmp(deviceStatePtr->fundingSourceBuffer, "LNbits") == 0);
    const char *rs = deviceStatePtr->rateSourceBuffer;
    String html = FPSTR(SETUP_PAGE_HTML);

    // Rate source dropdown — mark the active one as `selected`
    html.replace(F("%%RS_COINGECKO%%"),   (strcmp(rs, "CoinGecko")   == 0) ? "selected" : "");
    html.replace(F("%%RS_KRAKEN%%"),      (strcmp(rs, "Kraken")      == 0) ? "selected" : "");
    html.replace(F("%%RS_EXCHANGEAPI%%"), (strcmp(rs, "ExchangeApi") == 0) ? "selected" : "");
    html.replace(F("%%RS_COINYEP%%"),     (strcmp(rs, "CoinYEP")     == 0) ? "selected" : "");

    auto esc = [](const char* s) -> String {
      String out(s);
      out.replace(F("&"), F("&amp;"));
      out.replace(F("\""), F("&quot;"));
      out.replace(F("<"), F("&lt;"));
      out.replace(F(">"), F("&gt;"));
      return out;
    };

    // Build billmech CSV from vector (use macro alias directly to avoid double-expansion)
    String billsCsv;
    for (size_t i = 0; i < billAmountIntOne.size(); i++) {
      if (i > 0) billsCsv += ',';
      billsCsv += billAmountIntOne[i];
    }

    html.replace(F("%%WIFI_SSID%%"),      WiFi.isConnected() ? WiFi.SSID() : "");
    html.replace(F("%%CHECKED_BLINK%%"),  isLNbitsMode ? "" : "checked");
    html.replace(F("%%CHECKED_LNBITS%%"), isLNbitsMode ? "checked" : "");
    html.replace(F("%%BLINK_APIKEY%%"),   esc(blinkapikey));
    html.replace(F("%%BLINK_WALLET%%"),   esc(blinkwalletid));
    html.replace(F("%%ADMINKEY%%"),       esc(adminkey));
    html.replace(F("%%READKEY%%"),        esc(readkey));
    html.replace(F("%%LNURL_BASE%%"),     esc(baseURLATM1));
    html.replace(F("%%LNURL_SECRET%%"),   esc(secretATM1));
    html.replace(F("%%CUR1_CODE%%"),      esc(currencyOne));
    html.replace(F("%%CUR1_BILLS%%"),     billsCsv);
    html.replace(F("%%CUR1_MAX%%"),       String(maxamount, 0));
    html.replace(F("%%CUR1_CHARGE%%"),    String(charge1, 2));
    html.replace(F("%%ATM_TITLE%%"),      esc(atmtitle));
    html.replace(F("%%ATM_SUBTITLE%%"),   esc(atmsubtitle));
    html.replace(F("%%ATM_DESC%%"),       esc(atmdesc));
    // ap_password is not pre-filled — user must enter it explicitly to change it

    // Currencies 2 & 3 (LNbits only). Each lnurlN field is stored as CSV
    // "baseUrl,secret,code"; split on first two commas for the form.
    auto splitLnurlBase = [](const char* csv) -> String {
      String l(csv);
      const int c1 = l.indexOf(',');
      return (c1 > 0) ? l.substring(0, c1) : String();
    };
    auto splitLnurlSecret = [](const char* csv) -> String {
      String l(csv);
      const int c1 = l.indexOf(',');
      const int c2 = c1 >= 0 ? l.indexOf(',', c1 + 1) : -1;
      if (c1 > 0 && c2 > c1) return l.substring(c1 + 1, c2);
      return String();
    };
    auto billsCsvFromVec = [](const std::vector<int>& v) -> String {
      String s;
      for (size_t i = 0; i < v.size(); i++) { if (i) s += ','; s += v[i]; }
      return s;
    };

    html.replace(F("%%CUR2_CODE%%"),         esc(currencyTwo));
    html.replace(F("%%CUR2_LNURL_BASE%%"),   esc(splitLnurlBase(lnurl2).c_str()));
    html.replace(F("%%CUR2_LNURL_SECRET%%"), esc(splitLnurlSecret(lnurl2).c_str()));
    html.replace(F("%%CUR2_BILLS%%"),        billsCsvFromVec(billAmountIntTwo));
    html.replace(F("%%CUR2_MAX%%"),          String(maxamount2, 0));
    html.replace(F("%%CUR2_CHARGE%%"),       String(charge2, 2));

    html.replace(F("%%CUR3_CODE%%"),         esc(currencyThree));
    html.replace(F("%%CUR3_LNURL_BASE%%"),   esc(splitLnurlBase(lnurl3).c_str()));
    html.replace(F("%%CUR3_LNURL_SECRET%%"), esc(splitLnurlSecret(lnurl3).c_str()));
    html.replace(F("%%CUR3_BILLS%%"),        billsCsvFromVec(billAmountIntThree));
    html.replace(F("%%CUR3_MAX%%"),          String(maxamount3, 0));
    html.replace(F("%%CUR3_CHARGE%%"),       String(charge3, 2));

    // Firmware version (OTA upload form doesn't need a catalog fetch)
    html.replace(F("%%FW_VERSION%%"), F(FW_VERSION));

    server.send(200, "text/html", html);
  });

  server.on("/setup/save", HTTP_POST, [isApClient]() {
    if (!isApClient()) {
      server.send(403, "text/plain", "Setup only via AP");
      return;
    }

    // Save WiFi credentials if provided
    const String newSsid = server.arg("wifi_ssid");
    if (newSsid.length() > 0) {
      AutoConnectCredential cred;
      station_config_t sc;
      memset(&sc, 0, sizeof(sc));
      strlcpy((char*)sc.ssid,     newSsid.c_str(),                     sizeof(sc.ssid));
      strlcpy((char*)sc.password, server.arg("wifi_password").c_str(), sizeof(sc.password));
      sc.dhcp = STA_DHCP;
      cred.save(&sc);
    }

    // elements.json — [{name,value},...] array
    {
      DynamicJsonDocument doc(512);
      JsonArray arr = doc.to<JsonArray>();
      const String title = server.arg("atm_title");
      auto add = [&](const char* n, const String& v) {
        JsonObject o = arr.createNestedObject(); o["name"] = n; o["value"] = v;
      };
      const String newPwd = server.arg("ap_password");
      add("password",    newPwd.length() > 0 ? newPwd : String(deviceStatePtr->password));
      add("atmdesc",     server.arg("atm_desc"));
      add("atmsubtitle", server.arg("atm_subtitle"));
      add("atmtitle",    title.length() ? title : "FIAT HELL");
      File f = FlashFS.open(PARAM_FILE, "w");
      if (f) { serializeJson(doc, f); f.close(); }
    }

    // gui.json — update fundingSource and rateSource; preserve animated
    {
      GuiConfig gui;
      if (!configService.loadGuiConfig(FlashFS, GUI_FILE, gui)) {
        strlcpy(gui.rateSource, "CoinGecko", sizeof(gui.rateSource));
        strlcpy(gui.animated,   "No",        sizeof(gui.animated));
      }
      const String funding = server.arg("funding");
      strlcpy(gui.fundingSource,
              (funding == "LNbits") ? "LNbits" : "Blink",
              sizeof(gui.fundingSource));
      const String rs = server.arg("ratesource");
      if (rs == "CoinGecko" || rs == "Kraken" ||
          rs == "ExchangeApi" || rs == "CoinYEP") {
        strlcpy(gui.rateSource, rs.c_str(), sizeof(gui.rateSource));
      }
      configService.saveGuiConfig(FlashFS, GUI_FILE, gui);
    }

    // first.json — [{name,value},...] array matching ConfigService::loadFirst() order
    {
      DynamicJsonDocument doc(1024);
      JsonArray arr = doc.to<JsonArray>();
      const bool isLNbitsMode = (server.arg("funding") == "LNbits");
      const String lnurlVal =
          server.arg("lnurl_base") + "," +
          server.arg("lnurl_secret") + "," +
          server.arg("cur1_code");
      auto add = [&](const char* n, const String& v) {
        JsonObject o = arr.createNestedObject(); o["name"] = n; o["value"] = v;
      };
      add("blinkapikey",   isLNbitsMode ? "" : server.arg("blink_apikey"));
      add("blinkwalletid", isLNbitsMode ? "" : server.arg("blink_wallet"));
      add("lnurl",         isLNbitsMode ? lnurlVal : "");
      add("adminkey",      isLNbitsMode ? server.arg("adminkey") : "");
      add("readkey",       isLNbitsMode ? server.arg("readkey")  : "");
      add("currencyOne",   server.arg("cur1_code"));
      add("billmech",      server.arg("cur1_bills"));
      add("maxamount",     server.arg("cur1_max"));
      add("charge1",       server.arg("cur1_charge"));
      File f = FlashFS.open(FIRST_FILE, "w");
      if (f) { serializeJson(doc, f); f.close(); }
    }

    // second.json — write iff cur2_code is filled; otherwise delete the file
    {
      const String code = server.arg("cur2_code");
      if (code.length() > 0) {
        DynamicJsonDocument doc(1024);
        JsonArray arr = doc.to<JsonArray>();
        const String lnurlVal =
            server.arg("cur2_lnurl_base") + "," +
            server.arg("cur2_lnurl_secret") + "," + code;
        auto add = [&](const char* n, const String& v) {
          JsonObject o = arr.createNestedObject(); o["name"] = n; o["value"] = v;
        };
        add("currencyTwo", code);
        add("lnurl2",      lnurlVal);
        add("billmech2",   server.arg("cur2_bills"));
        add("maxamount2",  server.arg("cur2_max"));
        add("charge2",     server.arg("cur2_charge"));
        File f = FlashFS.open(SECOND_FILE, "w");
        if (f) { serializeJson(doc, f); f.close(); }
      } else {
        FlashFS.remove(SECOND_FILE);
      }
    }

    // third.json — same pattern
    {
      const String code = server.arg("cur3_code");
      if (code.length() > 0) {
        DynamicJsonDocument doc(1024);
        JsonArray arr = doc.to<JsonArray>();
        const String lnurlVal =
            server.arg("cur3_lnurl_base") + "," +
            server.arg("cur3_lnurl_secret") + "," + code;
        auto add = [&](const char* n, const String& v) {
          JsonObject o = arr.createNestedObject(); o["name"] = n; o["value"] = v;
        };
        add("currencyThree", code);
        add("lnurl3",        lnurlVal);
        add("billmech3",     server.arg("cur3_bills"));
        add("maxamount3",    server.arg("cur3_max"));
        add("charge3",       server.arg("cur3_charge"));
        File f = FlashFS.open(THIRD_FILE, "w");
        if (f) { serializeJson(doc, f); f.close(); }
      } else {
        FlashFS.remove(THIRD_FILE);
      }
    }

    server.send(200, "text/html",
      "<html><body style='background:#111;color:#eee;font-family:sans-serif;"
      "padding:32px;text-align:center'>"
      "<h2 style='color:#f90'>&#10003; Ulozene!</h2>"
      "<p>Zariadenie sa restartuje za 2 sekundy...</p>"
      "</body></html>");

    pendingRestartAt = millis() + 2000UL;
  });

  // Multipart firmware upload: phone POSTs .bin via the AP connection.
  // No internet needed on the device — bytes go straight into Update partition.
  server.on("/setup/ota", HTTP_POST,
    // Done handler — called after upload completes (success or failure)
    [isApClient]() {
      if (!isApClient()) { server.send(403, "text/plain", "AP only"); return; }
      const bool ok = !otaUploadAborted && !Update.hasError();
      String page = F("<html><body style='background:#111;color:#eee;font-family:sans-serif;padding:32px;text-align:center'>");
      if (ok) {
        page += F("<h2 style='color:#0c0'>&#10003; Firmware nahrat&yacute;</h2>"
                  "<p>Zariadenie sa re&scaron;tartuje za 2 sekundy&hellip;</p>");
      } else {
        page += F("<h2 style='color:#f33'>&#10007; Chyba</h2><p>");
        page += Update.errorString();
        page += F("</p><a href='/setup' style='color:#f90'>&#8592; Sp&auml;&#x165;</a>");
      }
      page += F("</body></html>");
      server.send(200, "text/html", page);
      if (otaUploadOverlay) {
        lv_obj_del(otaUploadOverlay);
        otaUploadOverlay = nullptr;
        lv_task_handler();
      }
      if (ok) pendingRestartAt = millis() + 2000UL;
    },
    // Upload handler — called repeatedly with chunks
    [isApClient]() {
      HTTPUpload& up = server.upload();
      if (up.status == UPLOAD_FILE_START) {
        otaUploadAborted = !isApClient();
        if (otaUploadAborted) return;
        Serial.printf("OTA upload begin: %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          otaUploadAborted = true;
          Update.printError(Serial);
          return;
        }
        otaUploadOverlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(otaUploadOverlay, screenWidth, screenHeight);
        lv_obj_set_pos(otaUploadOverlay, 0, 0);
        lv_obj_set_style_bg_color(otaUploadOverlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(otaUploadOverlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(otaUploadOverlay, 0, 0);
        lv_obj_clear_flag(otaUploadOverlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = lv_label_create(otaUploadOverlay);
        lv_label_set_text(l, "Nahravam firmware...");
        lv_obj_center(l);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_move_foreground(otaUploadOverlay);
        lv_task_handler();
      } else if (up.status == UPLOAD_FILE_WRITE && !otaUploadAborted) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
          otaUploadAborted = true;
          Update.printError(Serial);
        }
      } else if (up.status == UPLOAD_FILE_END && !otaUploadAborted) {
        if (!Update.end(true)) {  // true = set boot partition
          otaUploadAborted = true;
          Update.printError(Serial);
        } else {
          Serial.printf("OTA OK: %u bytes\n", up.totalSize);
        }
      } else if (up.status == UPLOAD_FILE_ABORTED) {
        otaUploadAborted = true;
        Update.abort();
      }
      yield();
    });

  // Block AutoConnect's portal pages from STA clients and redirect AP clients
  // to /setup. Registered before portal.begin() so these handlers take precedence.
  // Without this, /_ac/reset or /_ac/update would be reachable from public WiFi.
  {
    auto acGuard = [isApClient]() {
      const bool inConfigMode = portalRequestedByUser || portalRequiredForMissingConfig;
      if (!isApClient() && !inConfigMode) {
        server.send(403, "text/plain",
          "Config portal only via AP — hold BOOT button 3 s to enable");
        server.client().stop();
        return;
      }
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "");
      server.client().stop();
    };
    // AutoConnect internal portal UI
    server.on("/_ac",                acGuard);
    server.on("/_ac/",               acGuard);
    server.on("/_ac/config",         acGuard);
    server.on("/_ac/open",           acGuard);
    server.on("/_ac/savecredential", acGuard);
    server.on("/_ac/devinfo",        acGuard);
    server.on("/_ac/reset",          acGuard);
    server.on("/_ac/update",         acGuard);
    server.on("/_ac/update_do",      acGuard);
    // AutoConnect Aux pages registered via portal.join()
    server.on("/config",    acGuard);
    server.on("/elements",  acGuard);
    server.on("/save",      acGuard);
    server.on("/first",     acGuard);
    server.on("/savefirst", acGuard);
    server.on("/second",    acGuard);
    server.on("/savesecond",acGuard);
    server.on("/third",     acGuard);
    server.on("/savethird", acGuard);
    server.on("/gui",       acGuard);
    server.on("/savegui",   acGuard);
    server.on("/ota",       acGuard);
    server.on("/otado",     acGuard);
  }

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
    //} else {
    // triggerAp = true;
  }
  bootStage(31, "gui config loaded");

  guiAux.load(FPSTR(PAGE_GUI));
  guiAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    File paramGui = FlashFS.open(GUI_FILE, "r");
    if (paramGui) {
      aux.loadElement(paramGui, {"fundingsource", "ratesource", "animated"});
      paramGui.close();
    }

    if (portal.where() == "/gui") {
      File paramGui = FlashFS.open(GUI_FILE, "r");
      if (paramGui) {
        aux.loadElement(paramGui, {"fundingsource", "ratesource", "animated"});
        paramGui.close();
      }
    }
    return String();
  });
  bootStage(32, "gui aux configured");

  //*
  //*
  //*
  // Save page one
  saveAux.load(FPSTR(PAGE_SAVE));
  saveAux.on([](AutoConnectAux &aux, PageArgument &arg) {
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
                                    {"fundingsource", "ratesource", "animated"},
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
  }
  // Check if currencyATM3 is not empty
  if ((deviceState.currencyATM3[0] != '\0') || (currencyThree[0] != '\0')) {
    // Then merge billAmountIntThree into the now-extended billAmountIntOne
    billAmountIntOne.insert(billAmountIntOne.end(), billAmountIntThree.begin(),
                            billAmountIntThree.end());
  }
  bootStage(39, "bill vectors merged");

  /*********************************************************/
  /*** Set AutoConnect before launching the portal       ***/
  /*********************************************************/
  acConfig.auth = AC_AUTH_BASIC;
  acConfig.authScope = AC_AUTHSCOPE_PORTAL;
  acConfig.ticker = true;
  acConfig.autoReset = false;
  acConfig.autoReconnect = true;
  acConfig.retainPortal = true;
  acConfig.autoRise = false; // set dynamically during startup based on mode
  acConfig.apid = "LN ATM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  acConfig.psk = deviceState.password; // Password for AP
  acConfig.menuItems =
      AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS |
      AC_MENUITEM_DEVINFO | AC_MENUITEM_RESET | AC_MENUITEM_HOME;
  acConfig.title = "LN ATM";
  acConfig.homeUri = "/setup";
  acConfig.reconnectInterval = 1;
  acConfig.channel = 6;        // Fixed channel for stable AP (avoids scan disrupting clients)
  acConfig.beginTimeout = 12000; // 12 s — fast fallback to AP if saved WiFi unreachable
  acConfig.immediateStart =
      false; // If we don't have WiFi saved, it will start AP
  acConfig.username = "admin";
  acConfig.password = deviceState.password;
  bootStage(40, "autoconnect config prepared");

  // Register all Aux pages to the portal
  otaAux.load(FPSTR(PAGE_OTA));
  otaAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    // Populate version select from catalog
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient catalogHttp;
    catalogHttp.setTimeout(8000);
    if (catalogHttp.begin(client, OTA_CATALOG_URL)) {
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
            if (name && item["type"] == "bin") {
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
    String updateUrl = String(OTA_BASE_URL) + "/" + filename;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPUpdate updater;
    updater.setLedPin(-1); // Disable LED – GPIO2 is backlight on this display
    updater.rebootOnUpdate(true);
    // Show OTA overlay on display so user sees clear feedback (no blinking)
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
    updater.onStart([]() {
      if (otaOverlay) {
        lv_label_set_text(lv_obj_get_child(otaOverlay, 0),
                          "Downloading firmware...");
        lv_task_handler();
      }
    });
    updater.onProgress([](int curBytes, int totalBytes) {
      if (otaOverlay && totalBytes > 0) {
        int pct = (int)((100ULL * curBytes) / totalBytes);
        char buf[48];
        snprintf(buf, sizeof(buf), "Downloading... %d%%", pct);
        lv_label_set_text(lv_obj_get_child(otaOverlay, 0), buf);
      }
      lv_task_handler();
      yield();
    });
    HTTPUpdateResult updateResult = updater.update(client, updateUrl);
    if (otaOverlay) {
      lv_obj_del(otaOverlay);
      otaOverlay = nullptr;
      lv_task_handler();
    }
    if (updateResult == HTTP_UPDATE_OK) {
      aux["result"].value = "Update OK. Rebooting...";
      return String();
    }
    if (updateResult == HTTP_UPDATE_NO_UPDATES) {
      aux["result"].value = "No update available.";
      return String();
    }
    aux["result"].value =
        "Update failed: " + updater.getLastErrorString();
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

  const bool isBlinkMode =
      (strcmp(deviceState.fundingSourceBuffer, "Blink") == 0);
  const bool wifiRequired = isBlinkMode;
  const bool userWantsPortal = triggerAp; // tap during logo window

  const bool apiDataMissing =
      ((strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0 &&
        (deviceState.currencyATM[0] == '\0' || adminkey[0] == '\0' ||
         readkey[0] == '\0')) ||
       (strcmp(deviceState.fundingSourceBuffer, "Blink") == 0 &&
        (blinkapikey[0] == '\0' || blinkwalletid[0] == '\0')) ||
       (currencyOne[0] == '\0'));

  const bool shouldOpenPortalNow =
      (userWantsPortal || apiDataMissing || (wifiRequired && !wifiStatus()));
  const bool showPortalScreenImmediately =
      (userWantsPortal || apiDataMissing);

  portalRequestedByUser = userWantsPortal;
  portalRequiredForMissingConfig = apiDataMissing;
  portalRequiredForWifiRecovery = (wifiRequired && !wifiStatus());

  // In config-first mode, keep the AP stable for phones instead of trying to
  // reconnect to a remembered WiFi in the background.
  acConfig.autoReconnect = !(portalRequestedByUser || portalRequiredForMissingConfig);
  acConfig.preserveAPMode =
      (portalRequestedByUser || portalRequiredForMissingConfig);

  // Decide portal behavior once, then call portal.begin() once.
  acConfig.immediateStart = (userWantsPortal || apiDataMissing);
  acConfig.autoRise = (userWantsPortal || apiDataMissing || wifiRequired);
  exitCaptivePortalLoopOnce = shouldOpenPortalNow;
  portal.whileCaptivePortal(allowSetupToContinueWhilePortalStaysAlive);

  if (portalRequestedByUser || portalRequiredForMissingConfig) {
    Serial.println("Config portal requested: disconnecting STA to keep captive AP stable");
    WiFi.disconnect(false, false);
  }

  if (isBlinkMode) {
    Serial.println("Blink mode => Internet needed");
  } else {
    Serial.println("LNbits mode => offline possible");
  }

  if (userWantsPortal) {
    Serial.println("User tap => start AP portal immediately");
  } else if (apiDataMissing) {
    Serial.println("API data missing => start AP portal immediately");
  } else {
    Serial.println("No tap => try STA first");
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
  (void)portal.begin(); // may connect STA or start AP depending on config
  bootStage(47, "portal begin returned");

  if (wifiStatus()) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    if (wifiRequired) {
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
      if (MDNS.begin("fiathell")) {
        Serial.println("mDNS: http://fiathell.local (config AP)");
      }
      digitalWrite(11, LOW);
    }
  }
  bootStage(48, "wifi or portal state evaluated");

  // If portal is required (tap / missing data / Blink no-wifi), stay in portal.
  if (userWantsPortal || apiDataMissing || (wifiRequired && !wifiStatus())) {
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

// Create the logo screen
/**
 * @brief Sets the angle of an LVGL arc object.
 * This function is used to set the angle of an LVGL arc object.
 * @param obj Pointer to the LVGL arc object.
 * @param v The angle value to set.
 */
static void set_angle(void *obj, int32_t v) {
  lv_arc_set_value((lv_obj_t *)obj, v);
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
  lv_obj_t *arc = lv_arc_create(screen_logo); // Create the arc on screen_logo
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_obj_remove_style(arc, NULL,
                      LV_PART_KNOB); /*Be sure the knob is not displayed*/
  lv_obj_clear_flag(arc,
                    LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/
  lv_obj_center(arc);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_exec_cb(&a, set_angle);
  lv_anim_set_time(&a, 2000);
  lv_anim_set_repeat_count(&a, 1); /*Just for the demo*/
  lv_anim_set_repeat_delay(&a, 500);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_start(&a);

  lv_obj_t *img1 = lv_img_create(screen_logo); // Create an image object
  lv_img_set_src(
      img1,
      &btcSmallImg); // Set the image source to your converted image (my_image)
  lv_obj_align(img1, LV_ALIGN_CENTER, 0,
               0); // Align the image to the center of the screen

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

  /*String LVGL_PORTAL_URL = "http://fiathell.local  or  192.168.4.1";
  lv_obj_t *portalurl = lv_label_create(screen_portal);
  lv_label_set_text(portalurl, LVGL_PORTAL_URL.c_str());
  lv_obj_align(portalurl, LV_ALIGN_TOP_MID, 0, 198);
  lv_obj_set_style_text_font(portalurl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(portalurl, lv_color_hex(0x90EE90), 0);*/

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
  if (paymentService.isBlink(deviceState.fundingSourceBuffer)) {
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

  lv_scr_load(screen_thx);
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
  http.begin(String(coinyepConversionAPI) + targetCurrency);

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
  http.begin(exchangeapiConversionAPI);
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
  http.begin(String(coingeckoAPI) + curr);
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

  http.begin(String(krakenTickerAPI) + "?pair=" + pair);
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

/**
 * Checks the balance of the selected currency from the specified funding
 * source. If the funding source is "LNbits", it sends an HTTP GET request to
 * the LNbits API to retrieve the wallet balance. If the funding source is
 * "Blink", it sends a GraphQL query to the Blink API to fetch the wallet
 * information. The balance is then parsed from the response and converted to
 * the corresponding fiat currency value.
 */
void checkBalance() {
  if (strcmp(currencySelected, deviceState.currencyATM) == 0 ||
      strcmp(currencySelected, currencyOne) == 0) {
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM1, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM1, sizeof(secretATM));
    }
    chargeSelected = charge1;
    maxamountSelected = maxamount;
  } else if (strcmp(currencySelected, deviceState.currencyATM2) == 0 ||
             strcmp(currencySelected, currencyTwo) == 0) {
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM2, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM2, sizeof(secretATM));
    }
    chargeSelected = charge2;
    maxamountSelected = maxamount2;
  } else if (strcmp(currencySelected, deviceState.currencyATM3) == 0 ||
             strcmp(currencySelected, currencyThree) == 0) {
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM3, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM3, sizeof(secretATM));
    }
    chargeSelected = charge3;
    maxamountSelected = maxamount3;
  }
  if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
    http.begin(String(lnbitsURL) +
               "/api/v1/wallet");         // Specify request destination
    http.addHeader("X-Api-Key", readkey); // Specify API key header

    int httpCode = http.GET(); // Send the request

    if (httpCode == 200 || httpCode == 201) { // Check the returning code
      String responsePayload =
          http.getString(); // Get the request response payload
      Serial.println(responsePayload);

      // Parse JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, responsePayload);

      // Get balance from parsed JSON
      balanceSats = doc["balance"];

      fiatBalance = ((double)balanceSats * fiatValue) / 100000000000.0;

      ///*** Debug ***////
      /*Serial.print(F("Balance: "));
      Serial.println(fiatBalance);

      Serial.print(F("Currency: "));
      Serial.println(currencySelected);

      Serial.print(F("Conversion: "));
      Serial.println(fiatValue);
      Serial.print("Buffer: ");
      Serial.println(buffer);

      Serial.print(F("baseURLATM: "));
      Serial.println(baseURLATM);
      Serial.print(F("MAX selected: "));
      Serial.println(maxamountSelected);

      Serial.print(F("Balance Sats: "));
      Serial.println(balanceSats);
      Serial.print(F("HTTP (checkBalance): "));
      Serial.println(httpCode);
      Serial.print("Free heap (checkBalance): ");
      Serial.println(ESP.getFreeHeap());*/
    } else {
      Serial.print(F("Error (checkBalance): "));
      Serial.println(httpCode);
    }
  } else if (strcmp(deviceState.fundingSourceBuffer, "Blink") == 0) {
    http.begin(graphqlEndpoint); // API endpoint
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", String(blinkapikey)); // Correct API key header

    // GraphQL query
    String query = R"(
    query Me {
      me {
        defaultAccount {
          wallets {
            id
            walletCurrency
            balance
          }
        }
      }
    }
    )";

    // Prepare the JSON payload
    DynamicJsonDocument jsonDoc(1024);
    jsonDoc["query"] = query;

    String requestBody;
    serializeJson(jsonDoc,
                  requestBody); // Serialize the JSON object to a string

    // Send the POST request
    int httpCode = http.POST(requestBody);
    String responsePayload = http.getString(); // Get the response payload

    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);
    Serial.print("Response Payload: ");
    Serial.println(responsePayload);

    // Deserialize JSON response and extract wallet information
    DynamicJsonDocument respDoc(4096); // Adjust size based on expected response
    deserializeJson(respDoc, responsePayload);
    if (httpCode == 200) {
      JsonObject me = respDoc["data"]["me"]["defaultAccount"]["wallets"]
                             [0]; // Assuming you want the first wallet
      String walletIdStr = me["id"].as<String>();
      strlcpy(blinkwalletid, walletIdStr.c_str(), sizeof(blinkwalletid));
      String walletCurrency = me["walletCurrency"].as<String>();
      balanceSats = me["balance"];

      fiatBalance = ((double)balanceSats / 100000000.0) * fiatValue;

      ///*** Debug ***////
      /*Serial.print("Wallet ID: ");
      Serial.println(blinkwalletid);
      Serial.print("Wallet Currency: ");
      Serial.println(walletCurrency);
      Serial.print("Wallet Balance: ");
      Serial.println(balanceSats);
      Serial.print("Fiat balance: ");
      Serial.println(fiatBalance);*/
    } else {
      Serial.println("Failed to fetch wallet information");
    }
  }

  http.end(); // Close connection
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
void updateMainScreenLabel() {
  Serial.print("Free heap (updateMainScreenLabel Start): ");
  Serial.println(ESP.getFreeHeap());
  if (balanceValueLabel) { // Ensure the label has been created
    if (!wifiStatus()) {
      lv_label_set_text(balanceValueLabel, "OFFLINE");
    } else {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.2f %s", fiatBalance,
               currencySelected);
      lv_label_set_text(balanceValueLabel, buffer);
      lv_obj_set_style_text_color(balanceValueLabel, LV_COLOR_WHITE, 0);
      if (fiatBalance < maxamountSelected) {
        lv_obj_set_style_text_color(balanceValueLabel, LV_COLOR_RED, 0);
      }
    }
  }
  if (fiatValueLabel) { // Check if it has been initialized
    if (!wifiStatus()) {
      lv_label_set_text(fiatValueLabel, "OFFLINE");
      lv_obj_set_style_text_color(fiatValueLabel, LV_COLOR_RED, 0);
    } else {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%ld %s", (long)fiatValue,
               currencySelected);
      lv_label_set_text(fiatValueLabel, buffer);
      lv_obj_set_style_text_color(fiatValueLabel, LV_COLOR_GREEN, 0);
    }
  }
  if (chargeValueLabel) { // Check if it has been initialized
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f %%", (double)chargeSelected);
    lv_label_set_text(chargeValueLabel, buffer);
  }
  if (mainScreenCurrency1RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue1 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue1);
    else
      snprintf(buf, sizeof(buf), "-");
    lv_label_set_text(mainScreenCurrency1RateLabel, buf);
  }
  if (mainScreenCurrency1FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge1);
    lv_label_set_text(mainScreenCurrency1FeeLabel, feeBuf);
  }
  if (mainScreenCurrency2RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue2 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue2);
    else
      snprintf(buf, sizeof(buf), "-");
    lv_label_set_text(mainScreenCurrency2RateLabel, buf);
  }
  if (mainScreenCurrency2FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge2);
    lv_label_set_text(mainScreenCurrency2FeeLabel, feeBuf);
  }
  if (mainScreenCurrency3RateLabel) {
    char buf[32];
    if (wifiStatus() && sessionState.fiatValue3 > 0)
      snprintf(buf, sizeof(buf), "%ld", (long)sessionState.fiatValue3);
    else
      snprintf(buf, sizeof(buf), "-");
    lv_label_set_text(mainScreenCurrency3RateLabel, buf);
  }
  if (mainScreenCurrency3FeeLabel) {
    char feeBuf[24];
    snprintf(feeBuf, sizeof(feeBuf), "Fee: %.1f%%", (double)charge3);
    lv_label_set_text(mainScreenCurrency3FeeLabel, feeBuf);
  }

  Serial.print("Free heap (updateMainScreenLabel End): ");
  Serial.println(ESP.getFreeHeap());
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
    lv_anim_set_time(&a, 500); // duration of one color change cycle
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
      buffer, sizeof(buffer), "%.2f %s", fiatBalance,
      currencySelected); // Limiting to 2 decimal places and append the currency
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
  }

  lv_scr_load(screen_main);
  attachBatteryToCurrentScreen();
  Serial.println("createMainScreen: Screen loaded");
  // Mixed-currency mode: accept all bills, no single-currency filter
#if BILL_ACCEPTOR_ENABLED
  uninhibitAllChannels();
  enableAcceptor();
#endif
  Serial.print("Free heap (createMainScreen End): ");
  Serial.println(ESP.getFreeHeap());
}

void enableAcceptor() {
  if (paymentService.isBlink(deviceState.fundingSourceBuffer) &&
      (!wifiStatus())) {
    Serial.println("Error: Blink API is selected but the device is offline");
    return;
  }
  billAcceptorWrite(184);  // Enable acceptor (channels already set by setCurrency)
  if (INHIBITMECH >= 0) {
    digitalWrite(INHIBITMECH, HIGH); // Uninhibit currencies
  }
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
void createInsertMoneyScreen() {
  isInsertingMoney = true;

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
    lv_label_set_text(labelLastInserted, ""); // Initialize with empty text
    lv_obj_align(labelLastInserted, LV_ALIGN_TOP_LEFT, 30, 50);
    lv_obj_set_style_text_font(labelLastInserted, &lv_font_montserrat_24, 0);
  } else {
    Serial.println("Failed to create labelLastInserted!");
  }

  // Create label for displaying the total amount (single-currency) or empty (mixed)
  labelTotalAmount = lv_label_create(screen_insert_money);
  if (labelTotalAmount) {
    lv_label_set_text(labelTotalAmount, "");
    lv_obj_align(labelTotalAmount, LV_ALIGN_TOP_LEFT, 30, 100);
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

  // Mixed-currency: total in sats
  labelTotalSats = lv_label_create(screen_insert_money);
  if (labelTotalSats) {
    lv_label_set_text(labelTotalSats, "");
    lv_obj_align(labelTotalSats, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(labelTotalSats, &lv_font_montserrat_48, 0);
  }

  // Create prompt label
  lv_obj_t *labelPrompt = lv_label_create(screen_insert_money);
  if (labelPrompt) {
    lv_label_set_text(labelPrompt, "TAP SCREEN WHEN FINISHED");
    lv_obj_align(labelPrompt, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_text_font(labelPrompt, &lv_font_montserrat_16, 0);
  } else {
    Serial.println("Failed to create labelPrompt!");
  }

  // Create label for displaying the maximum amount
  labelMaxAmount = lv_label_create(screen_insert_money);
  if (labelMaxAmount) {
    lv_label_set_text(labelMaxAmount, ""); // Initialize with empty text
    lv_obj_align(labelMaxAmount, LV_ALIGN_TOP_MID, 0, 330);
    lv_obj_set_style_text_font(labelMaxAmount, &lv_font_montserrat_16, 0);
  } else {
    Serial.println("Failed to create labelMaxAmount!");
  }

  // Load the new screen
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
  if (suspendTouchPolling) {
    data->state = LV_INDEV_STATE_REL;
    return;
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
  if (callback[0] == '\0') {
    Serial.println("Error: callback URL is empty");
    return false;
  }

  http.begin(callback); // Initialize the connection to the URL
  http.addHeader("Content-Type", "application/json"); // Set header for JSON
  http.setTimeout(5000); // Set timeout to 5 seconds

  int httpCode = http.GET(); // Perform the GET request

  if (httpCode == 200) { // Check if the request was successful
    String responseCallback = http.getString(); // Get the response as a string

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, responseCallback);

    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    const char *inv = doc["invoice"];
    if (inv && strlen(inv) > 0) {
      strlcpy(sessionState.boltInvoice, inv, sizeof(sessionState.boltInvoice));
      Serial.print("Bolt Invoice received: ");
      Serial.println(sessionState.boltInvoice);
      http.end();
      return true;
    } else {
      Serial.println("Invoice not found in the JSON response.");
      http.end();
      return false;
    }
  } else {
    // Not ready yet, this is normal - invoice hasn't been generated
    http.end();
    return false;
  }
}

/**
 * Sends a POST request to the GraphQL API endpoint with the provided Bolt
 * invoice. The request includes the necessary headers and payload to process
 * the payment.
 *
 * @param boltInvoice The Bolt invoice to be sent as part of the request
 * payload.
 */
void getBlinkLnURL(const char *invoice) {
  http.begin(graphqlEndpoint); // Initialize with the API endpoint
  http.addHeader("Content-Type", "application/json"); // Set content type
  http.addHeader("X-API-KEY",
                 blinkapikey); // Add the API key in the Authorization header

  // Prepare the GraphQL mutation as a string
  String graphqlQuery = R"(
    mutation LnInvoicePaymentSend($input: LnInvoicePaymentInput!) {
        lnInvoicePaymentSend(input: $input) {
            status
            errors {
                message
                path
                code
            }
        }
    })";

  // Prepare the JSON payload
  DynamicJsonDocument doc(1024);
  doc["query"] = graphqlQuery;
  doc["variables"]["input"]["walletId"] = blinkwalletid;
  doc["variables"]["input"]["paymentRequest"] = invoice;
  doc["variables"]["input"]["memo"] = "LightningATM payout";

  String requestBody;
  serializeJson(doc, requestBody); // Serialize JSON document to a string

  // Make the POST request
  int httpCode = http.POST(requestBody);     // Send the request
  String responsePayload = http.getString(); // Get the response payload

  Serial.print("Modified LNURL: ");
  Serial.println(sessionState.modifiedLnURLgen);
  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);
  Serial.print("Response Payload: ");
  Serial.println(responsePayload);

  http.end(); // Close connection
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
 * @param None
 * @return None
 */
void createLNURLWithdraw() {
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 | sessionState.totalCurrency3) != 0;
  if (mixed) {
    result = computeMixedTotalSats();
    tempCharge = 0.0f;
    Serial.print("Mixed-currency result (sats): ");
    Serial.println(result);
  } else {
    float temp = ((total / 100.0) / fiatValue * 1e8);
    result = (long)round(temp * (100.0f - chargeSelected) / 100.0f);
    tempCharge = temp - (float)result;
  }
  Serial.print("Result (after fee, satoshis): ");
  Serial.println(result);

  String resultStr = String(result);

  http.begin(primaryApiEndpoint);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["amount"] = result;        // Set the amount to withdraw in satoshis
  doc["memo"] = "Fiat Hell ATM"; // Set the memo for the withdrawal

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.print("requestBody: ");
  Serial.println(requestBody);

  int httpCode = http.POST(requestBody);
  if (httpCode != 200 && httpCode != 201) {
    // Primary service failed, try secondary service
    Serial.println("Primary service failed with code: " + String(httpCode));
    Serial.println("Attempting to connect to secondary service...");

    http.end(); // End connection to primary service
    http.begin(secondaryApiEndpoint);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(requestBody);
  }

  if (httpCode == 200 || httpCode == 201) {
    String responsePayload = http.getString();
    Serial.print("Blink payload: ");
    Serial.println(responsePayload);
    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, responsePayload);

    // Get balance from parsed JSON
    strlcpy(lnURLgen, doc["lnurl"] | "", sizeof(lnURLgen));
    if (strlen(lnURLgen) > 10) {
      strlcpy(sessionState.modifiedLnURLgen, lnURLgen + 10,
              sizeof(sessionState.modifiedLnURLgen));
    } else {
      sessionState.modifiedLnURLgen[0] = '\0';
    }
    strlcpy(callback, doc["callback"] | "", sizeof(callback));
  } else {
    Serial.println("Failed to generate LNURL: " + String(httpCode));
  }

  http.end();
}

/**
 * @brief Retrieves the Blink LNURL and executes an operation using the LNURL.
 *
 * This function retrieves the Blink LNURL and performs an operation using the
 * LNURL. It calculates the total amount in cents, the EUR value (price of 1
 * Bitcoin in euros), and the charge. It then converts the total amount to
 * satoshis and subtracts the charge if applicable. Finally, it sends a POST
 * request to the API endpoint with the LNURL and retrieves the response from
 * Blink.
 *
 * @note Make sure to set the appropriate values for `total`, `fiatValue`,
 * `chargeSelected`, `graphqlEndpoint`, `blinkapikey`, and `lnurl` before
 * calling this function.
 */
void getBlinkLNURL() {
  Serial.print("Total (cents): ");
  Serial.println(total);
  Serial.print("EUR Value (price of 1 Bitcoin in euros): ");
  Serial.println(fiatValue);
  Serial.print("Charge: ");
  Serial.println(chargeSelected);

  float temp = ((total / 100.0) / fiatValue * 1e8);
  result = (long)round(temp * (100.0f - chargeSelected) / 100.0f);
  tempCharge = temp - (float)result;

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);
  Serial.print("Charge %: ");
  Serial.println(chargeSelected);
  Serial.print("Result (after fee, satoshis): ");
  Serial.println(result);

  String resultStr = String(result);

  http.begin(graphqlEndpoint); // API endpoint
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", blinkapikey); // Correct API key header

  String graphqlQuery = R"(
    mutation UseLNURL($input: LNURLInput!) {
        executeLNURLOperation(input: $input) {
            result
            status
            message
        }
    })";

  DynamicJsonDocument doc(1024);
  doc["query"] = graphqlQuery;
  doc["variables"]["input"]["lnurl"] = lnurl;

  String requestBody;
  serializeJson(doc, requestBody);
  int httpCode = http.POST(requestBody);
  if (httpCode == 201) {
    String response = http.getString();
    Serial.println("Response from Blink: " + response);
  } else {
    Serial.println("Failed to execute operation via Blink: " +
                   String(httpCode));
  }

  http.end();
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
  return sessionState.fiatValue1 > 0 ? sessionState.fiatValue1 : 0.0f;
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
 * @brief Compute total satoshis from mixed-currency totals (fee and rate per currency).
 */
static long computeMixedTotalSats() {
  long totalSats = 0;
  if (sessionState.totalCurrency1 > 0 && sessionState.fiatValue1 > 0) {
    float afterFee = (sessionState.totalCurrency1 / 100.0f) * (100.0f - charge1) / 100.0f;
    totalSats += (long)round(afterFee / sessionState.fiatValue1 * 1e8);
  }
  if (sessionState.totalCurrency2 > 0 && sessionState.fiatValue2 > 0) {
    float afterFee = (sessionState.totalCurrency2 / 100.0f) * (100.0f - charge2) / 100.0f;
    totalSats += (long)round(afterFee / sessionState.fiatValue2 * 1e8);
  }
  if (sessionState.totalCurrency3 > 0 && sessionState.fiatValue3 > 0) {
    float afterFee = (sessionState.totalCurrency3 / 100.0f) * (100.0f - charge3) / 100.0f;
    totalSats += (long)round(afterFee / sessionState.fiatValue3 * 1e8);
  }
  return totalSats;
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
void getLNURL() {
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 | sessionState.totalCurrency3) != 0;
  float temp = 0.0f;
  if (mixed) {
    result = computeMixedTotalSats();
    temp = (float)result;
    tempCharge = 0.0f;
    Serial.print("Mixed-currency result (sats): ");
    Serial.println(result);
  } else {
    Serial.print("Total (cents): ");
    Serial.println(total);
    Serial.print("EUR Value (price of 1 Bitcoin in euros): ");
    Serial.println(fiatValue);
    Serial.print("Charge: ");
    Serial.println(chargeSelected);
    temp = ((total / 100.0) / fiatValue * 1e8);
    result = (long)round(temp * (100.0f - chargeSelected) / 100.0f);
    tempCharge = temp - (float)result;
  }

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);
  Serial.print("Charge %: ");
  Serial.println(chargeSelected);
  Serial.print("Result (after fee, satoshis): ");
  Serial.println(result);

  String resultStr = String(result);

  if (lnbitsURL[0] == '\0') {
    Serial.println("Error: lnbitsURL is empty in getLNURL");
    return;
  }

  http.end(); // Ensure previous connection is closed

  char requestUrl[512];
  snprintf(requestUrl, sizeof(requestUrl), "%s/withdraw/api/v1/links",
           lnbitsURL);
  http.begin(requestUrl); // Specify request destination
  http.addHeader("Content-Type",
                 "application/json");    // Specify content-type header
  http.addHeader("X-Api-Key", adminkey); // Specify API key header

  String httpRequestData = "{\"title\": \"Fiat Hell ";
  httpRequestData += "\", \"min_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"max_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"uses\": 1, \"wait_time\": 1, \"is_unique\": 1, "
                     "\"webhook_url\": \"\"}";
  int httpCode = http.POST(httpRequestData);

  // int httpCode = http.POST("{\"title\": \"Fiat Hell\", \"min_withdrawable\":
  // \" + result + \", \"max_withdrawable\": \" + result + \" , \"uses\": \"1\",
  // \"wait_time\": \"1\", \"is_unique\": \"true\", \"webhook_url\": \"\"}"); //
  // Send the request
  String responsePayload = http.getString(); // Get the response payload

  // Serial.println(httpCode);   // Print HTTP return code
  Serial.print("Temp: ");
  Serial.println(temp); // Print request response payload
  Serial.print("Result: ");
  Serial.println(result);
  Serial.print("ResultSTR: ");
  Serial.println(resultStr);
  Serial.print("Payload: ");
  Serial.println(responsePayload); // Print request response payload

  // Parse JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, responsePayload);

  // Get balance from parsed JSON
  strlcpy(lnURLgen, doc["lnurl"] | "", sizeof(lnURLgen));

  Serial.print("LNURL: ");
  Serial.println(lnURLgen);
  strlcpy(sessionState.modifiedLnURLgen, lnURLgen,
          sizeof(sessionState.modifiedLnURLgen));

  http.end(); // Close connection
  // lv_task_handler();
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
void makeLNURL() {
  int randomPin = random(1000, 9999);
  byte nonce[8];
  for (int i = 0; i < 8; i++) {
    nonce[i] = random(256);
  }

  // Mixed: encode total sats; single currency: encode amount after fee (cents)
  uint64_t amountToEncode;
  const bool mixed = (sessionState.totalCurrency1 | sessionState.totalCurrency2 | sessionState.totalCurrency3) != 0;
  if (mixed) {
    result = computeMixedTotalSats();
    amountToEncode = (uint64_t)result;
  } else {
    float amountAfterFeeCents = total * (100.0f - chargeSelected) / 100.0f;
    amountToEncode = (uint64_t)round(amountAfterFeeCents);
  }

  byte payload[51]; // 51 bytes is max one can get with xor-encryption

  size_t payload_len = xor_encrypt(
      payload, sizeof(payload), (uint8_t *)secretATM, strlen(secretATM), nonce,
      sizeof(nonce), randomPin, amountToEncode);
  String preparedURL = String(baseURLATM) + "?atm=1&p=";
  preparedURL +=
      toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);

  Serial.println(preparedURL);
  char Buf[200];
  preparedURL.toCharArray(Buf, 200);
  char *url = Buf;
  byte *data = (byte *)calloc(strlen(url) * 2, sizeof(byte));
  size_t len = 0;
  int res = convert_bits(data, &len, 5, (byte *)url, strlen(url), 8, 1);
  char *charLnurl = (char *)calloc(strlen(url) * 2, sizeof(byte));
  bech32_encode(charLnurl, "lnurl", data, len);
  to_upper(charLnurl);
  strlcpy(qrData, charLnurl, sizeof(qrData));
  Serial.print("Buf: ");
  Serial.println(Buf);
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
  lv_obj_center(qr);

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

  // Load the screen
  lv_scr_load(screen_qr);
  attachBatteryToCurrentScreen();

  // Debugging heap memory
  Serial.print("Free heap (showQRCodeLVGL): ");
  Serial.println(ESP.getFreeHeap());

  printHeapStatus(); // Print heap status for debugging
}

void printHeapStatus() {
  Serial.print("Total heap: ");
  Serial.println(ESP.getHeapSize());
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Largest free block: ");
  Serial.println(ESP.getMaxAllocHeap());
  Serial.print("Heap fragmentation: ");
  Serial.println((float)(ESP.getHeapSize() - ESP.getFreeHeap()) /
                 ESP.getHeapSize() * 100.0);
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
// Kept for forward compatibility but no longer called from runtime.
// Portal mode is now triggered exclusively at boot (logo-tap or auto-detect).
static unsigned long configModeActiveUntil = 0;

void triggerRuntimeConfigMode() {
  if (configModeActiveUntil) return; // already active
  portalRequestedByUser   = true;
  acConfig.preserveAPMode = true;
  acConfig.autoReconnect  = false;
  portal.config(acConfig);
  if (!(WiFi.getMode() & WIFI_AP)) WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(acConfig.apid.c_str(), acConfig.psk.c_str(), acConfig.channel);
  configModeActiveUntil = millis() + 5UL * 60UL * 1000UL;
  Serial.println("Config mode active: " + acConfig.apid + " -> 192.168.4.1  (5 min)");
}

void handleUiStateMachine() {
  unsigned long currentTime = millis();
  BTNA.read();


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
        // Invoice received! Process it and show thank you screen
        Serial.println("Blink invoice received => processing payment");
        getBlinkLnURL(sessionState.boltInvoice);
        uiController.deleteQRCodeScreen();
        createThankYouScreen();
        lv_task_handler();
        currentUiState = UI_THANK_YOU;
        stateEnterTime = millis();
        isBlinkFlow = false;
      }
      // If no invoice yet, continue polling (will check again in 2 seconds)
    }
    // Optional: Add timeout (e.g., 5 minutes) to prevent infinite waiting
    if (currentTime - stateEnterTime >= 300000) { // 5 minutes timeout
      Serial.println("Blink invoice timeout => restarting");
      ESP.restart();
    }
    break;

  case UI_WAITING_FOR_TAP: {
    // Non-blocking wait for tap after QR code (for LNbits)
    uint16_t qrTouchX, qrTouchY;
    if (BTNA.wasPressed() || lcd.getTouch(&qrTouchX, &qrTouchY)) {
      // Reset for the next transaction
      coins = 0;
      bills = 0;
      total = 0;
      isInsertingMoney = false;
      currentUiState = UI_IDLE;
      Serial.println("Tap detected => resetting and restarting");
      ESP.restart();
    }
    break;
  }

  case UI_THANK_YOU: {
    // Thank you screen: 5 seconds or tap to continue
    uint16_t thxTouchX, thxTouchY;
    bool thxTap = lcd.getTouch(&thxTouchX, &thxTouchY) || BTNA.wasPressed();
    if (thxTap || (currentTime - stateEnterTime >= 5000)) {
      Serial.println("Thank you => restarting");
      ESP.restart();
    }
    break;
  }

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
    if (WiFi.getMode() & WIFI_AP) WiFi.mode(WIFI_STA);
    Serial.println("Config mode timed out — AP closed");
  }

  lv_timer_handler();    // Let the GUI do its work
  portal.handleClient(); // Already non‑blocking

  if (pendingConfigReload) {
    reloadRuntimeConfigFromFlash();
  }

  const bool portalActive = portal.isPortalAvailable();
  suspendTouchPolling = portalActive;

  if (pendingPortalCompletion && wifiStatus()) {
    if (portalRequestedByUser || portalRequiredForMissingConfig) {
      if (!portalNetworkStateLogged) {
        if (!(WiFi.getMode() & WIFI_AP)) {
          Serial.println("Re-enabling config AP alongside STA");
          WiFi.mode(WIFI_AP_STA);
          WiFi.softAP(acConfig.apid.c_str(), acConfig.psk.c_str());
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
    lv_task_handler();
  }

  if (initialCheck) {
    previousMillis =
        millis() - interval; // So that it gets executed immediately after setup
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
    int channelIdx = x - 1;  // 0-based
    int amount = billAmountIntOne[channelIdx];
    const bool mixed = (sessionState.allowedChannelCount == (int)billAmountIntOne.size());
    bool creditBill = false;

    if (mixed) {
      // Mixed-currency: accept all, add to per-currency totals
      creditBill = true;
      if (channelIdx < (int)originalSizeOne) {
        sessionState.totalCurrency1 += (long)amount * 100;
        strlcpy(sessionState.lastBillCurrency, currencyOne, sizeof(sessionState.lastBillCurrency));
      } else if (channelIdx < (int)(originalSizeOne + originalSizeTwo)) {
        sessionState.totalCurrency2 += (long)amount * 100;
        strlcpy(sessionState.lastBillCurrency, currencyTwo, sizeof(sessionState.lastBillCurrency));
      } else {
        sessionState.totalCurrency3 += (long)amount * 100;
        strlcpy(sessionState.lastBillCurrency, currencyThree, sizeof(sessionState.lastBillCurrency));
      }
      sessionState.lastBillCents = (long)amount * 100;
    } else {
      // Single-currency (user tapped a button): only credit if channel matches
      if (channelIdx >= sessionState.allowedChannelStart &&
          channelIdx < sessionState.allowedChannelStart + sessionState.allowedChannelCount) {
        creditBill = true;
      }
    }

    if (creditBill) {
      if (!mixed) {
        bills = bills + amount;
        total = (coins + bills);
      }
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
        long totalSats = 0;
        if (labelTotalCurrency1 && sessionState.totalCurrency1 > 0) {
          long sats1 = 0;
          if (sessionState.fiatValue1 > 0) {
            float afterFee = (sessionState.totalCurrency1 / 100.0f) * (100.0f - charge1) / 100.0f;
            sats1 = (long)round(afterFee / sessionState.fiatValue1 * 1e8);
            totalSats += sats1;
          }
          snprintf(buf, sizeof(buf), "Total %s: %.2f %s (%ld sats)", currencyOne,
                  sessionState.totalCurrency1 / 100.0f, currencyOne, sats1);
          lv_label_set_text(labelTotalCurrency1, buf);
        } else if (labelTotalCurrency1) {
          lv_label_set_text(labelTotalCurrency1, "");
        }
        if (labelTotalCurrency2 && sessionState.totalCurrency2 > 0) {
          long sats2 = 0;
          if (sessionState.fiatValue2 > 0) {
            float afterFee = (sessionState.totalCurrency2 / 100.0f) * (100.0f - charge2) / 100.0f;
            sats2 = (long)round(afterFee / sessionState.fiatValue2 * 1e8);
            totalSats += sats2;
          }
          snprintf(buf, sizeof(buf), "Total %s: %.2f %s (%ld sats)", currencyTwo,
                  sessionState.totalCurrency2 / 100.0f, currencyTwo, sats2);
          lv_label_set_text(labelTotalCurrency2, buf);
        } else if (labelTotalCurrency2) {
          lv_label_set_text(labelTotalCurrency2, "");
        }
        if (labelTotalCurrency3 && sessionState.totalCurrency3 > 0) {
          long sats3 = 0;
          if (sessionState.fiatValue3 > 0) {
            float afterFee = (sessionState.totalCurrency3 / 100.0f) * (100.0f - charge3) / 100.0f;
            sats3 = (long)round(afterFee / sessionState.fiatValue3 * 1e8);
            totalSats += sats3;
          }
          snprintf(buf, sizeof(buf), "Total %s: %.2f %s (%ld sats)", currencyThree,
                  sessionState.totalCurrency3 / 100.0f, currencyThree, sats3);
          lv_label_set_text(labelTotalCurrency3, buf);
        } else if (labelTotalCurrency3) {
          lv_label_set_text(labelTotalCurrency3, "");
        }
        if (labelTotalSats) {
          snprintf(buf, sizeof(buf), "Total: %ld sats", totalSats);
          lv_label_set_text(labelTotalSats, buf);
        }
        lv_label_set_text(labelTotalAmount, "");
        if (labelMaxAmount) {
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
        String lastBillString = "Last bill: " + String(amount) + " " + currencySelected;
        String totalString = "Total: " + String(total) + " " + currencySelected;
        String maxString = "MAX: " + String(maxamountSelected) + " " +
                           currencySelected + " from " +
                           deviceState.fundingSourceBuffer;
        lv_label_set_text(labelLastInserted, lastBillString.c_str());
        lv_label_set_text(labelTotalAmount, totalString.c_str());
        lv_label_set_text(labelMaxAmount, maxString.c_str());
        if (labelTotalCurrency1) lv_label_set_text(labelTotalCurrency1, "");
        if (labelTotalCurrency2) lv_label_set_text(labelTotalCurrency2, "");
        if (labelTotalCurrency3) lv_label_set_text(labelTotalCurrency3, "");
        if (labelTotalSats) lv_label_set_text(labelTotalSats, "");
      }
    }
  }
  // Check button release, touchscreen tap, or total (only if in INSERTING_MONEY state)
  const bool hasMixed = (sessionState.totalCurrency1 || sessionState.totalCurrency2 || sessionState.totalCurrency3) != 0;
  const bool hasAmount = (total != 0) || hasMixed;
  if (currentUiState == UI_INSERTING_MONEY) {
    uint16_t touchX, touchY;
    bool screenTapped = hasAmount && lcd.getTouch(&touchX, &touchY);
    if ((BTNA.wasPressed() && hasAmount) || screenTapped || mixedLimitExceededAutoProceed || (!hasMixed && total >= maxamountSelected)) {
      mixedLimitExceededAutoProceed = false;
      if (hasMixed) {
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

      if (!wifiStatus()) {
        uiController.deleteInsertMoneyScreen();
        Serial.println("deleteInsertMoneyScreen() - LNbits offline: ");
        makeLNURL();
        printHeapStatus();
        Serial.println("makeLNURL() - LNbits offline: ");
        showQRCodeLVGL(qrData);
        Serial.print("showQRCodeLVGL() - LNbits offline: ");
        Serial.println(qrData);
        // Turn off machines
        billAcceptorWrite(185);
        if (INHIBITMECH >= 0) {
          digitalWrite(INHIBITMECH, LOW);
        }
        Serial.print("Free heap (makeLNURL): ");
        Serial.println(ESP.getFreeHeap());
        lv_task_handler();
        Serial.println("lv_task_handler() - LNbits offline");
        currentUiState = UI_SHOWING_QR;
        stateEnterTime = millis();
        qrDebounceDone = false;
      } else {
        if (paymentService.isBlink(deviceState.fundingSourceBuffer)) {
          uiController.deleteInsertMoneyScreen();
          Serial.println("deleteInsertMoneyScreen() - Blink online");
          createLNURLWithdraw();
          Serial.println("createLNURLWithdraw() - Blink online");
          // Display the QR code for online
          showQRCodeLVGL(lnURLgen);
          Serial.println("showQRCodeLVGL() - Blink online");
          lv_task_handler();
          Serial.println("lv_task_handler() - Blink online");
          // Turn off machines
          billAcceptorWrite(185);
          if (INHIBITMECH >= 0) {
            digitalWrite(INHIBITMECH, LOW);
          }
          currentUiState = UI_SHOWING_QR;
          stateEnterTime = millis();
          qrDebounceDone = false;
          isBlinkFlow = true; // Mark that we're in Blink flow
        }
        if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
          if (paymentService.hasLNbitsConfig(lnbitsURL, adminkey, readkey)) {
            uiController.deleteInsertMoneyScreen();
            Serial.println("deleteInsertMoneyScreen() - LNbits online");
            getLNURL();
            Serial.println("getLNURL()");
            // Display the QR code for online
            showQRCodeLVGL(lnURLgen);
            Serial.println("showQRCodeLVGL() - LNbits online");
            lv_task_handler();
            Serial.println("lv_task_handler() - LNbits online");
            // Turn off machines
            billAcceptorWrite(185);
            if (INHIBITMECH >= 0) {
              digitalWrite(INHIBITMECH, LOW);
            }
            currentUiState = UI_SHOWING_QR;
            stateEnterTime = millis();
            qrDebounceDone = false;
          } else {
            uiController.deleteInsertMoneyScreen();
            Serial.println(
                "deleteInsertMoneyScreen() - LNbits offline fallback");
            makeLNURL();
            showQRCodeLVGL(qrData);
            lv_task_handler();
            billAcceptorWrite(185);
            if (INHIBITMECH >= 0) {
              digitalWrite(INHIBITMECH, LOW);
            }
            currentUiState = UI_SHOWING_QR;
            stateEnterTime = millis();
            qrDebounceDone = false;
          }
        }
        Serial.print("Free heap (showQRCodeLVGL): ");
        Serial.println(ESP.getFreeHeap());
      }
    }

    lv_task_handler(); // Call LVGL task handler
    yield();           // Yield to other tasks (non-blocking, prevents watchdog)
  }
}
