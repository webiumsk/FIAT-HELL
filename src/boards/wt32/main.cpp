//= == == == == == == == == == == == == == == == == == == == == == == == == ==
//== == = //
//============EDIT IF USING DIFFERENT HARDWARE============//
//========================================================//
// v0.1

bool format = false; // true for formatting FOSSA memory, use once, then make
                     // false and reflash

#define BTN1 39 // Screen tap button

#define RX1 32 // Bill acceptor
#define TX1 33 // Bill acceptor

#define TX2 4         // Coinmech
#define INHIBITMECH 2 // Coinmech

//========================================================//
//========================================================//
//========================================================//

#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

#include <LovyanGFX.hpp> // main library
static LGFX lcd;         // declare display variable

#include "lv_conf.h"
#include "lv_font_montserrat_bold_60.c"
#include "lv_font_the_bold_48.c"
#include <lvgl.h>

static const lv_color_t colors[] = {LV_COLOR_PURPLE, LV_COLOR_RED,   LV_COLOR_ORANGE,
                       LV_COLOR_YELLOW, LV_COLOR_GREEN, LV_COLOR_BLUE};

#include <FS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

using WebServerClass = WebServer;
fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true

#include <AutoConnect.h>
#define AUTOCONNECT_USE_LOG 1
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <JC_Button.h>
#include <SPI.h>

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
#include "services/FundingService.h"
#include "services/PaymentService.h"
#include "services/UiController.h"

// Global device state (persistent configuration)
static DeviceState deviceState;

// Global session state (runtime state)
static SessionState sessionState;

HTTPClient http; // Declare object of class HTTPClient

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

lv_obj_t *btn1; // Currencies buttons
lv_obj_t *btn2;
lv_obj_t *btn3;

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
const char *fundingsource = deviceState.fundingSourceBuffer;
const char *ratesource = deviceState.rateSourceBuffer;
const char *animated = deviceState.enableAnimBuffer;

// UI objects (remain global as they're LVGL objects)
lv_obj_t *balanceValueLabel = nullptr;
lv_obj_t *fiatValueLabel = nullptr;
lv_obj_t *chargeValueLabel = nullptr;

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

// Galoy (Blink/Flash) and LNURL-proxy endpoints live in FundingService.
const String coingeckoConversionAPI =
    "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=";
const String exchangeapiConversionAPI =
    "https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@latest/v1/"
    "currencies/btc.json"; // https://github.com/fawazahmed0/exchange-api
const String cuexConversionAPI = "https://api.cuex.com/v1/exchanges/btc";
const String cuexApiKey =
    "3b71e5d431b2331acb65f2d484d423e5"; // Replace with your actual API key
const String alternativeConversionAPI =
    "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=";

WiFiClientSecure secureClient;

// Blink and Flash both speak the Galoy GraphQL API; FundingService holds the
// endpoints and client code.
static bool isGaloySource() {
  return FundingService::isGaloy(deviceState.fundingSourceBuffer);
}

HardwareSerial SerialPort1(1);
HardwareSerial SerialPort2(2);

Button BTNA(BTN1);

lv_obj_t *screen_logo, *screen_portal, *screen_api, *screen_thx, *screen_main,
    *screen_insert_money, *screen_qr, *screen_currency;
lv_obj_t *labelbtn;
lv_obj_t *fiathell;
lv_obj_t *labelLastInserted = nullptr;
lv_obj_t *labelTotalAmount = nullptr;
lv_obj_t *labelMaxAmount = nullptr;
lv_obj_t *wait_label = nullptr; // Status label on currency screen
//lv_obj_t *btn_reset = nullptr;   // Back button on currency screen

lv_obj_t *loadingLabel;

static GuiConfig guiConfig;
static ConfigService configService;
static PaymentService paymentService;
static UiController uiController(screen_logo, screen_portal, screen_api,
                                 screen_thx, screen_main, screen_insert_money,
                                 screen_qr, screen_currency);

// Switch fundingsource
lv_obj_t *switch_label;
lv_obj_t *switch_fund;
lv_obj_t *rate_label;
lv_obj_t *switch_rate;
lv_obj_t *anim_label;
lv_obj_t *img_blink;
lv_obj_t *img_lnbits;

void checkStackUsage() {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.printf("Stack high water mark: %u bytes\n", highWaterMark);
}

/* ----------------------------------
-------------- PORTAL ---------------
-----------------------------------*/

bool triggerAp = false;

String content = "<h1>ATM Access-point</br>For easy variable setting</h1>";

#include "pagefirst.h"
#include "pagegui.h"
#include "pageone.h"
#include "pagesecond.h"
#include "pagethird.h"

WebServerClass server;
AutoConnect portal(server);
AutoConnectConfig config;
AutoConnectAux elementsAux;
AutoConnectAux saveAux;
AutoConnectConfig first;
AutoConnectAux firstAux;
AutoConnectAux savefirstAux;
AutoConnectConfig second;
AutoConnectAux secondAux;
AutoConnectAux savesecondAux;
AutoConnectConfig third;
AutoConnectAux thirdAux;
AutoConnectAux savethirdAux;
AutoConnectConfig gui;
AutoConnectAux guiAux;
AutoConnectAux saveguiAux;

/*** Setup screen resolution for LVGL ***/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;
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
void createCurrencyScreen(const char *currency, float rate, float balance,
                          float charge);
void createInsertMoneyScreen();
void createSwitch(lv_obj_t *parent);
void lv_button_currency();
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

void checkNetworkAndDeviceStatus();
void createPaymentErrorScreen();
void startConfigPortal();
//void btn_reset_event_handler(lv_event_t *e);
void handleUiStateMachine();

//void createResetButton(lv_obj_t *parent);
void printHeapStatus();
void createLoadingIndicator();
void showLoadingIndicator();
void hideLoadingIndicator();
void enableAcceptor();

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

void setup() {
  /*********************/
  /*** Init display ***/
  /*********************/
  lcd.init(); // Initialize LovyanGFX
  lv_init();  // Initialize lvgl

  // Set orientation (landscape)
  if (lcd.width() < lcd.height()) {
    lcd.setRotation(lcd.getRotation() ^ 1);
  }

  // LVGL buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  // Set LVGL display
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Set LVGL input (touch)
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // Logo / Splash screen
  createLogoScreen();
  lv_task_handler(); // refresh obrazovky

  /*********************************/
  /*** Initialize periferies  ***/
  /*********************************/
  BTNA.begin(); // Button/Screen touch
  Serial.begin(115200);
  delay(10);

  SerialPort1.begin(300, SERIAL_8N2, TX1, RX1); // Bill acceptor
  SerialPort2.begin(4800, SERIAL_8N1, TX2);     // Coin mech
  pinMode(INHIBITMECH, OUTPUT);

  secureClient.setInsecure();

  // Start logo wait state (non-blocking)
  currentUiState = UI_LOGO_WAIT;
  stateEnterTime = millis();

  // Non-blocking wait for tap during logo screen
  // Keep checking for tap while loading config
  while (currentUiState == UI_LOGO_WAIT) {
    lv_task_handler();
    handleUiStateMachine();
    yield(); // Allow other tasks to run
  }

  /******************************************/
  /*** Read params from SPIFFS  ***/
  /******************************************/
  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
  if (format == true) {
    SPIFFS.format();
  }

  // Serial config upload window: web installer sends WRITE_CONFIG:/file.json:{json}
  // then CONFIG_DONE. Device writes files to SPIFFS and continues normal boot.
  // Only blocks if data arrives within 2 s — normal boots are not delayed.
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
              String filePath = line.substring(13, colon2);
              String fileContent = line.substring(colon2 + 1);
              File f = SPIFFS.open(filePath, "w");
              if (f) {
                f.print(fileContent);
                f.close();
              }
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

    const JsonObject conf0 = conf[0];
    const char *conf0Char = conf0["value"];
    strlcpy(deviceState.password, conf0Char, sizeof(deviceState.password));

    const JsonObject conf1 = conf[1];
    const char *conf1Char = conf1["value"];
    strlcpy(atmdesc, conf1Char, sizeof(atmdesc));

    const JsonObject conf2 = conf[2];
    const char *conf2Char = conf2["value"];
    strlcpy(atmsubtitle, conf2Char, sizeof(atmsubtitle));

    const JsonObject conf3 = conf[3];
    const char *conf3Char = conf3["value"];
    strlcpy(atmtitle, conf3Char, sizeof(atmtitle));
    //} else {
    // triggerAp = true;
  }
  paramFile.close();

  server.on("/", []() {
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content);
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

  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
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

    // A /gui.json saved by older firmware carries only the Blink/LNbits
    // options and would hide Flash after loadElement - re-add it.
    AutoConnectRadio &fundingRadio = aux["fundingsource"].as<AutoConnectRadio>();
    bool hasFlash = false;
    for (size_t i = 0; i < fundingRadio.size(); i++) {
      if (fundingRadio.at(i) == "Flash") {
        hasFlash = true;
        break;
      }
    }
    if (!hasFlash) {
      fundingRadio.add("Flash");
    }
    return String();
  });

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
    return String();
  });

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
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });

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
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });

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
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });

  // Save gui page
  saveguiAux.load(FPSTR(GUI_SAVE));
  saveguiAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    aux["caption"].value = GUI_FILE;
    String echo;
    if (configService.saveAuxConfig(FlashFS, GUI_FILE, guiAux,
                                    {"fundingsource", "ratesource", "animated"},
                                    echo)) {
      aux["echo"].value = echo;
    } else {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String();
  });

  originalSizeOne = billAmountIntOne.size();
  originalSizeTwo = billAmountIntTwo.size();
  originalSizeThree = billAmountIntThree.size();

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

  /*********************************************************/
  /*** Set AutoConnect before launching the portal       ***/
  /*********************************************************/
  config.auth = AC_AUTH_BASIC;
  config.authScope = AC_AUTHSCOPE_AUX;
  config.ticker = true;
  config.autoReconnect = true;
  config.autoRise = false; // set dynamically during startup based on mode
  config.apid = "LN ATM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  config.psk = deviceState.password; // Password for AP
  config.menuItems =
      AC_MENUITEM_CONFIGNEW | AC_MENUITEM_DEVINFO | AC_MENUITEM_RESET;
  config.title = "LN ATM";
  config.reconnectInterval = 1;
  config.immediateStart =
      false; // If we don't have WiFi saved, it will start AP
  // To define a username/password for the Basic Auth portal, you can use:
  config.username = deviceState.password;
  config.password = deviceState.password;

  // Register all Aux pages to the portal
  portal.join({elementsAux, saveAux, firstAux, savefirstAux, secondAux,
               savesecondAux, thirdAux, savethirdAux, guiAux, saveguiAux});

  // Apply config
  portal.config(config);

  // Create the loading indicator
  createLoadingIndicator();
  lv_task_handler();
  delay(5);

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

  const bool isGaloyMode = isGaloySource();
  const bool wifiRequired = isGaloyMode;
  const bool userWantsPortal = triggerAp; // tap during logo window

  const bool apiDataMissing =
      ((strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0 &&
        (deviceState.currencyATM[0] == '\0' || adminkey[0] == '\0' ||
         readkey[0] == '\0')) ||
       (isGaloyMode &&
        (blinkapikey[0] == '\0' || blinkwalletid[0] == '\0')) ||
       (currencyOne[0] == '\0'));

  // Decide portal behavior once, then call portal.begin() once.
  config.immediateStart = (userWantsPortal || apiDataMissing);
  config.autoRise = (userWantsPortal || apiDataMissing || wifiRequired);

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
  } else if (apiDataMissing) {
    Serial.println("API data missing => start AP portal immediately");
  } else {
    Serial.println("No tap => try STA first");
  }

  portal.config(config);
  Serial.println("Attempting to connect via AutoConnect...");
  (void)portal.begin(); // may connect STA or start AP depending on config

  if (wifiStatus()) {
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    if (wifiRequired) {
      // If you don't want to leave the AP on, switch to STA only
      WiFi.mode(WIFI_STA);
    }
  } else {
    Serial.println("WiFi not connected.");
    if (config.autoRise) {
      Serial.println("Portal available. AP Name: " + config.apid);
      digitalWrite(11, LOW);
    }
  }

  // If portal is required (tap / missing data / Blink no-wifi), stay in portal.
  if (userWantsPortal || apiDataMissing || (wifiRequired && !wifiStatus())) {
    return;
  }

  // Otherwise we can continue (LNbits offline allowed).
  Serial.println("Proceeding to main screen");
  createMainScreen();
  lv_task_handler();

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
    // Fallback if structure not matched, though usually should match if valid
    // URL
    strlcpy(lnbitsURL, baseURLATM, sizeof(lnbitsURL));
  }
  Serial.print(F("lnbitsURL: "));
  Serial.println(lnbitsURL); // This should print "https://your.lnbits.com"
  Serial.print("ESP Free heap (Setup end): ");
  Serial.println(ESP.getFreeHeap());
}

/**
 * Reads a single byte from the SerialPort1 if data is available.
 * This function is non-blocking, meaning it returns immediately
 * whether data is available or not.
 *
 * @return The byte read from the SerialPort1, or -1 if no data is available.
 */
int nonBlockingRead() {
  if (SerialPort1.available()) {
    return SerialPort1.read();
  }
  return -1; // No data available
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
  // lv_obj_set_style_text_color(atmurl, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_ONE = "Find new Wi-Fi network 'LN ATM-xxxx' ";
  lv_obj_t *portaltextone =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextone,
                    LVGL_PORTAL_TEXT_ONE.c_str()); // set label text
  lv_obj_align(portaltextone, LV_ALIGN_TOP_MID, 0,
               120); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextone, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_TWO = "in your phone and connect to it. After ";
  lv_obj_t *portaltexttwo =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltexttwo,
                    LVGL_PORTAL_TEXT_TWO.c_str()); // set label text
  lv_obj_align(portaltexttwo, LV_ALIGN_TOP_MID, 0,
               160); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltexttwo, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_THREE = "you are connected, open ATM settings ";
  lv_obj_t *portaltextthree =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextthree,
                    LVGL_PORTAL_TEXT_THREE.c_str()); // set label text
  lv_obj_align(portaltextthree, LV_ALIGN_TOP_MID, 0,
               200); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextthree, &lv_font_montserrat_22,
                             0); // Use the large font

  String LVGL_PORTAL_TEXT_FOUR = "and set your preferences";
  lv_obj_t *portaltextfour =
      lv_label_create(screen_portal); // full screen as the parent
  lv_label_set_text(portaltextfour,
                    LVGL_PORTAL_TEXT_FOUR.c_str()); // set label text
  lv_obj_align(portaltextfour, LV_ALIGN_TOP_MID, 0,
               240); // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextfour, &lv_font_montserrat_22,
                             0); // Use the large font

  lv_scr_load(screen_portal);
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
      SerialPort1.write(185);
      digitalWrite(INHIBITMECH, LOW);
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
}

/**
 * @brief Creates a payment error screen.
 *
 * Shown when the funding source rejected the payout after cash was already
 * inserted, so the customer must not walk away thinking they were paid.
 */
void createPaymentErrorScreen() {
  lv_obj_t *screen_err = lv_obj_create(NULL); // Create a new screen

  lv_obj_t *errTitle = lv_label_create(screen_err);
  lv_label_set_text(errTitle, "PAYMENT FAILED!");
  lv_obj_align(errTitle, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_text_font(errTitle, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(errTitle, LV_COLOR_RED, 0);

  lv_obj_t *errDesc = lv_label_create(screen_err);
  lv_label_set_text(errDesc,
                    "YOUR SATS WERE NOT SENT\nMAKE A PHOTO AND CONTACT SUPPORT");
  lv_obj_set_style_text_align(errDesc, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(errDesc, LV_ALIGN_CENTER, 0, 50);
  lv_obj_set_style_text_font(errDesc, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(errDesc, LV_COLOR_RED, 0);

  lv_scr_load(screen_err);
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

  // Skip inhibit/uninhibit if acceptor is not enabled (e.g., at startup)
  if (skipInhibit) {
    Serial.println("setCurrency: Skipping inhibit (acceptor not enabled)");
    return;
  }

  // Clear all channels before setting the new ones
  // Reduced delay - bill acceptor should respond faster
  // Most bill acceptors can handle commands much faster than 200ms
  for (int i = 0; i < 16; i++) {
    SerialPort1.write(INHIBIT_START + i); // Inhibit all initially
    delay(5); // Reduced from 200ms to 5ms - much faster
  }

  // Determine which channels to uninhibit based on the selected currency
  int startChannel = 0;
  int currencySize = 0;

  if (strcmp(currencySelected, currencyOne) == 0) {
    startChannel = 0;
    currencySize = originalSizeOne;
  } else if (strcmp(currencySelected, currencyTwo) == 0) {
    startChannel = originalSizeOne;
    currencySize = originalSizeTwo;
  } else if (strcmp(currencySelected, currencyThree) == 0) {
    startChannel = originalSizeOne + originalSizeTwo;
    currencySize = originalSizeThree;
  }

  // Uninhibit channels for the selected currency
  // Reduced delay - commands can be sent faster
  for (int i = 0; i < currencySize; i++) {
    int channelCode = UNINHIBIT_START + startChannel + i;
    Serial.print("Sending value allow ");
    Serial.print(currencySelected);
    Serial.print(": ");
    Serial.println(channelCode);
    SerialPort1.write(channelCode);
    delay(
        2); // Reduced from 20ms to 2ms - minimal delay for serial communication
  }
}

void checkPrice() {
  if (strcmp(deviceState.rateSourceBuffer, "ExchangeApi") == 0) {
    checkPriceExchangeApi();
  } else {
    // Accepts both "CoinGecko" (shared config) and legacy "Coingecko".
    // CoinYEP/Kraken are only implemented on the S3 board; fall back here.
    if (strcmp(deviceState.rateSourceBuffer, "CoinYEP") == 0 ||
        strcmp(deviceState.rateSourceBuffer, "Kraken") == 0) {
      Serial.println("Rate source not supported on WT32, using CoinGecko");
    }
    checkPriceCoinGecko();
  }
}

void checkPriceCoinGecko() {
  // Ask for the USD rate too - the Flash USD wallet needs the cross rate
  http.begin(coingeckoConversionAPI + currencySelected +
             ",usd"); // Specify request destination

  int httpCode = http.GET(); // Send the request

  if (httpCode == 200 || httpCode == 201) // Check the returning code
  {
    String responsePayload =
        http.getString(); // Get the request response payload
    // Serial.println(responsePayload);
    //  Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, responsePayload);

    String tempCurrency = currencySelected;
    tempCurrency.toLowerCase();

    // Get EUR value from parsed JSON
    fiatValue = doc["bitcoin"][tempCurrency.c_str()];
    // USD cross rate rides along in the same response (Flash USD wallet)
    float usd = doc["bitcoin"]["usd"] | 0.0f;
    if (usd > 0.0f) {
      sessionState.btcUsdValue = usd;
    }
    Serial.print(F("HTTP (checkPriceCoinGecko): "));
    Serial.println(httpCode);
  } else {
    Serial.print(F("Error (checkPriceCoinGecko): "));
    Serial.println(httpCode);
  }
  Serial.print("Free heap (checkPriceCoinGecko): ");
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
      // USD cross rate for the Flash USD wallet (response carries all rates)
      float usd = doc["btc"]["usd"] | 0.0f;
      if (usd > 0.0f) {
        sessionState.btcUsdValue = usd;
      }

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
  } else if (isGaloySource()) {
    // Blink pays from the BTC wallet; Flash from the custodial USD wallet
    // (its BTC wallet is external/non-custodial - server can't spend it).
    const char *walletCur =
        FundingService::galoyWalletCurrency(deviceState.fundingSourceBuffer);
    if (FundingService::fetchGaloyBalance(http, deviceState, sessionState,
                                          walletCur)) {
      if (strcmp(walletCur, "USD") == 0) {
        // Balance is in USD cents; convert to the operator's fiat via the
        // BTC/USD cross rate (usd_cents/100 * (fiat_per_btc / usd_per_btc)).
        if (sessionState.btcUsdValue > 0.0f) {
          fiatBalance = ((double)balanceSats / 100.0) *
                        (fiatValue / sessionState.btcUsdValue);
        } else {
          fiatBalance = (double)balanceSats / 100.0; // raw USD as fallback
          Serial.println("BTC/USD rate missing - balance shown in USD");
        }
      } else {
        fiatBalance = ((double)balanceSats / 100000000.0) * fiatValue;
      }
    }
    return; // FundingService closed its connection already
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

/**
 * @brief Event handler for button 1.
 *
 * This function is called when button 1 is clicked. It sets the currency to
 * currencyOne, updates the base URL and secret if the funding source is
 * "LNbits", and updates the charge and max amount variables. It then shows the
 * currency screen with the updated values.
 *
 * @param e The event object.
 */
static void btn1_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    showLoadingIndicator();

    // Set currency first (fast operation)
    setCurrency(currencyOne);

    // Update config variables
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM1, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM1, sizeof(secretATM));
    }
    chargeSelected = charge1;
    maxamountSelected = maxamount;

    // Show screen immediately with current values (may be slightly outdated)
    createCurrencyScreen(currencyOne, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();
    hideLoadingIndicator();

    // Update price and balance in background (these are slow HTTP requests)
    // This happens after screen is shown, so user sees response immediately
    checkPrice();
    checkBalance();

    // Update screen with new values after they're fetched
    createCurrencyScreen(currencyOne, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();

    // Enable acceptor only after currency is set, price and balance are loaded,
    // and inhibit channels are configured
    enableAcceptor();

    // Update status label to "READY" with green color
    if (wait_label != nullptr) {
      lv_label_set_text(wait_label, "READY");
      lv_obj_set_style_text_color(wait_label, lv_color_hex(0x00FF00),
                                  0); // Green color
      lv_task_handler();
    }

    Serial.print("Currency set to ");
    Serial.println(currencyOne);
  }
}

/**
 * @brief Event handler for button 2.
 *
 * This function is called when button 2 is clicked. It performs the following
 * actions:
 * 1. Shows a loading indicator.
 * 2. Sets the currency to currencyTwo.
 * 3. Prints a message to the serial monitor indicating the currency set.
 * 4. Updates the baseURLATM and secretATM variables based on the funding source
 * buffer.
 * 5. Sets the chargeSelected variable to charge2.
 * 6. Sets the maxamountSelected variable to maxamount2.
 * 7. Shows the currency screen with the updated values.
 * 8. Hides the loading indicator.
 *
 * @param e The event object.
 */
static void btn2_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    showLoadingIndicator();

    // Set currency first (fast operation)
    setCurrency(currencyTwo);

    // Update config variables
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM2, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM2, sizeof(secretATM));
    }
    chargeSelected = charge2;
    maxamountSelected = maxamount2;

    // Show screen immediately with current values (may be slightly outdated)
    createCurrencyScreen(currencyTwo, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();
    hideLoadingIndicator();

    // Update price and balance in background (these are slow HTTP requests)
    checkPrice();
    checkBalance();

    // Update screen with new values after they're fetched
    createCurrencyScreen(currencyTwo, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();

    // Enable acceptor only after currency is set, price and balance are loaded,
    // and inhibit channels are configured
    enableAcceptor();

    // Update status label to "READY" with green color
    if (wait_label != nullptr) {
      lv_label_set_text(wait_label, "READY");
      lv_obj_set_style_text_color(wait_label, lv_color_hex(0x00FF00),
                                  0); // Green color
      lv_task_handler();
    }

    Serial.print("Currency set to ");
    Serial.println(currencyTwo);
  }
}

/**
 * Event handler for button 3.
 * This function is called when button 3 is clicked.
 * It sets the currency to currencyThree, updates the base URL and secret if the
 * funding source is LNbits, sets the chargeSelected and maxamountSelected
 * variables, and shows the currency screen with the new values.
 */
static void btn3_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    showLoadingIndicator();

    // Set currency first (fast operation)
    setCurrency(currencyThree);

    // Update config variables
    if (strcmp(deviceState.fundingSourceBuffer, "LNbits") == 0) {
      strlcpy(baseURLATM, baseURLATM3, sizeof(baseURLATM));
      strlcpy(secretATM, secretATM3, sizeof(secretATM));
    }
    chargeSelected = charge3;
    maxamountSelected = maxamount3;

    // Show screen immediately with current values (may be slightly outdated)
    createCurrencyScreen(currencyThree, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();
    hideLoadingIndicator();

    // Update price and balance in background (these are slow HTTP requests)
    checkPrice();
    checkBalance();

    // Update screen with new values after they're fetched
    createCurrencyScreen(currencyThree, fiatValue, fiatBalance, chargeSelected);
    lv_task_handler();

    // Enable acceptor only after currency is set, price and balance are loaded,
    // and inhibit channels are configured
    enableAcceptor();

    // Update status label to "READY" with green color
    if (wait_label != nullptr) {
      lv_label_set_text(wait_label, "READY");
      lv_obj_set_style_text_color(wait_label, lv_color_hex(0x00FF00),
                                  0); // Green color
      lv_task_handler();
    }

    Serial.print("Currency set to ");
    Serial.println(currencyThree);
  }
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
    snprintf(buffer, sizeof(buffer), "%.1f %%", chargeSelected);
    lv_label_set_text(chargeValueLabel, buffer);
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
  uiController.deleteCurrencyScreen();
  lv_task_handler();
  SerialPort1.write(185); // Command to turn off the acceptor
  digitalWrite(INHIBITMECH, LOW);

  Serial.println("createMainScreen: Start machine");
  Serial.print("Free heap (createMainScreen Start): ");
  Serial.println(ESP.getFreeHeap());

  screen_main = lv_obj_create(NULL); // Create a new screen
  Serial.println("createMainScreen: Screen created");

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
  };
  lv_obj_t *zeroline =
      lv_label_create(screen_main); // full screen as the parent
  lv_label_set_text(zeroline, LVGL_Zero_Title.c_str()); // set label text
  lv_obj_align(zeroline, LV_ALIGN_TOP_MID, 0, 45); // Center but 20 from the top
  if (strcmp(atmsubtitle, "AMITY") == 0 || strcmp(atmsubtitle, "Amity") == 0) {
    lv_label_set_text(zeroline, ""); // set label text
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
  lv_obj_align(fiathell, LV_ALIGN_TOP_MID, 0, 95); // Center but 95 from the top
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
               163); // Center but 163 from the top
  Serial.println("createMainScreen: burnTextLabel created");

  if (strcmp(atmsubtitle, "DVADSATJEDEN") == 0 ||
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
           chargeSelected); // chargeSelected is a float percentage
  chargeValueLabel =
      lv_label_create(screen_main); // Create it on your main screen
  lv_label_set_text(chargeValueLabel,
                    buffer); // Set the label text using the buffer
  lv_obj_align(chargeValueLabel, LV_ALIGN_BOTTOM_RIGHT, -30,
               -20); // Position it as you like
  lv_obj_set_style_text_font(chargeValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: chargeValueLabel created");

  lv_button_currency();
  Serial.println("createMainScreen: lv_button_currency created");
  //}
  img_blink = lv_img_create(screen_main);
  lv_img_set_src(
      img_blink,
      &blink); // 'blink' must be a properly defined LVGL image variable
  lv_obj_align(img_blink, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);

  img_lnbits = lv_img_create(screen_main);
  lv_img_set_src(
      img_lnbits,
      &lnbits); // 'lnbits' must be a properly defined LVGL image variable
  lv_obj_align(img_lnbits, LV_ALIGN_TOP_LEFT, 10, 10);
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
    lv_obj_align(flashLabel, LV_ALIGN_TOP_LEFT, 10, 10);
  }

  lv_scr_load(screen_main);
  Serial.println("createMainScreen: Screen loaded");
  Serial.print("Free heap (createMainScreen End): ");
  Serial.println(ESP.getFreeHeap());
}

/**
 * Displays the currency screen with the given currency, rate, balance, and
 * charge.
 *
 * @param currency The selected currency.
 * @param rate The rate of the currency in BTC.
 * @param balance The balance in the selected currency.
 * @param charge The fee percentage.
 */
void createCurrencyScreen(const char *currency, float rate, float balance,
                          float charge) {
  // Delete existing currency screen if it exists
  if (screen_currency != nullptr) {
    lv_obj_del(screen_currency);
    screen_currency = nullptr;
    wait_label = nullptr; // Reset wait_label reference
    //btn_reset = nullptr;   // Reset btn_reset reference
  }

  // Create new currency screen
  screen_currency = lv_obj_create(NULL);

  String currency_text = "Selected Currency: " + String(currency);
  lv_obj_t *currency_label = lv_label_create(screen_currency);
  lv_label_set_text(currency_label, currency_text.c_str());
  lv_obj_align(currency_label, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_text_font(currency_label, &lv_font_montserrat_16, 0);

  String rate_text = "Rate: " + String(rate) + " " + currency + "/BTC";
  lv_obj_t *rate_label = lv_label_create(screen_currency);
  lv_label_set_text(rate_label, rate_text.c_str());
  lv_obj_align(rate_label, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_set_style_text_font(currency_label, &lv_font_montserrat_16, 0);

  String balance_text = "Balance: " + String(balance) + " " + currency;
  lv_obj_t *balance_label = lv_label_create(screen_currency);
  lv_label_set_text(balance_label, balance_text.c_str());
  lv_obj_align(balance_label, LV_ALIGN_TOP_MID, 0, 80);
  lv_obj_set_style_text_font(currency_label, &lv_font_montserrat_16, 0);

  String charge_text = "Fee: " + String(charge) + "%";
  lv_obj_t *charge_label = lv_label_create(screen_currency);
  lv_label_set_text(charge_label, charge_text.c_str());
  lv_obj_align(charge_label, LV_ALIGN_TOP_MID, 0, 110);
  lv_obj_set_style_text_font(charge_label, &lv_font_montserrat_16, 0);

  String insert_text = "INSERT " + String(currency) + " SHITCOIN";
  lv_obj_t *insert_label = lv_label_create(screen_currency);
  lv_label_set_text(insert_label, insert_text.c_str());
  lv_obj_align(insert_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(insert_label, &lv_font_montserrat_24, 0);

  // Create status label (WAITING FOR ACCEPTOR... / READY)
  // Delete existing wait_label if it exists
  if (wait_label != nullptr) {
    lv_obj_del(wait_label);
    wait_label = nullptr;
  }

  String wait_text = "WAITING FOR ACCEPTOR...";
  wait_label = lv_label_create(screen_currency);
  lv_label_set_text(wait_label, wait_text.c_str());
  lv_obj_align(wait_label, LV_ALIGN_TOP_MID, 0, 200);
  lv_obj_set_style_text_font(wait_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(wait_label, LV_COLOR_ORANGE, 0);

  //createResetkButton(screen_currency);

  // Enable reset button when currency screen is created (in case it was
  // disabled)
  /*if (btn_reset != nullptr) {
    lv_obj_clear_state(btn_reset, LV_STATE_DISABLED);
  }*/

  lv_scr_load(screen_currency);

  // Note: enableAcceptor() is now called in button handlers after all data is
  // loaded
}

void enableAcceptor() {
  if (paymentService.isGaloy(deviceState.fundingSourceBuffer) &&
      (!wifiStatus())) {
    Serial.println(
        "Error: online funding source selected but the device is offline");
    return;
  } else {
    SerialPort1.write(184);          // Enable acceptor
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
  uiController.deleteCurrencyScreen(); // Properly manage deletion of the
                                       // previous screen

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

  // Create label for displaying the total amount
  labelTotalAmount = lv_label_create(screen_insert_money);
  if (labelTotalAmount) {
    lv_label_set_text(labelTotalAmount, ""); // Initialize with empty text
    lv_obj_align(labelTotalAmount, LV_ALIGN_TOP_LEFT, 30, 100);
    lv_obj_set_style_text_font(labelTotalAmount, &lv_font_montserrat_48, 0);
  } else {
    Serial.println("Failed to create labelTotalAmount!");
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
    lv_obj_align(labelMaxAmount, LV_ALIGN_TOP_LEFT, 30, 220);
    lv_obj_set_style_text_font(labelMaxAmount, &lv_font_montserrat_16, 0);
  } else {
    Serial.println("Failed to create labelMaxAmount!");
  }

  // Load the new screen
  lv_scr_load(screen_insert_money);
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
 * @brief Function to create and initialize currency buttons.
 *
 * This function creates and initializes currency buttons (up to 3 currencies).
 * It sets the position, size, and text of each button based on the currency
 * values. It also sets the button style for the checked state and sets the
 * initial currency.
 * @note This function assumes that the variables currencyATM3, currencyThree,
 * currencyOne, currencyTwo, and currencySelected are defined and accessible.
 */
void lv_button_currency() {
  lv_obj_t *labelbtn;

  // Initialize styles
  static lv_style_t style_btn_default, style_btn_pressed;
  lv_style_init(&style_btn_default);
  lv_style_init(&style_btn_pressed);

  // Default style properties
  lv_style_set_bg_color(&style_btn_default, lv_color_black());
  lv_style_set_border_color(&style_btn_default, LV_COLOR_ORANGE);
  lv_style_set_border_width(&style_btn_default, 2);

  // Checked style properties
  lv_style_set_bg_color(&style_btn_pressed, LV_COLOR_PURPLE);
  lv_style_set_border_color(&style_btn_pressed, LV_COLOR_PURPLE);
  lv_style_set_border_width(&style_btn_pressed, 2);

  // Calculate positions based on the number of buttons
  int num_buttons = 0;
  if (currencyOne[0] != '\0')
    num_buttons++;
  if (currencyTwo[0] != '\0')
    num_buttons++;
  if (currencyThree[0] != '\0')
    num_buttons++;

  int screen_width = 480;
  int btn_width = 120;
  int btn_height = 50;
  int spacing = 20;

  int start_x =
      (screen_width - (num_buttons * btn_width + (num_buttons - 1) * spacing)) /
      2;

  // Create buttons and apply styles
  if (currencyOne[0] != '\0') {
    btn1 = lv_btn_create(screen_main);
    lv_obj_add_style(btn1, &style_btn_default, 0); // Apply default style
    lv_obj_add_style(btn1, &style_btn_pressed,
                     LV_STATE_PRESSED); // Apply checked style
    lv_obj_add_event_cb(btn1, btn1_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn1, start_x, 200);
    lv_obj_set_size(btn1, btn_width, btn_height);
    labelbtn = lv_label_create(btn1);

    if (currencyTwo[0] == '\0' && currencyThree[0] == '\0') {
      lv_label_set_text(labelbtn, "START");
    } else {
      lv_label_set_text(labelbtn, currencyOne);
    }

    lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
    lv_obj_center(labelbtn);

    start_x += btn_width + spacing;
  }

  if (currencyTwo[0] != '\0') {
    btn2 = lv_btn_create(screen_main);
    lv_obj_add_style(btn2, &style_btn_default, 0);
    lv_obj_add_style(btn2, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn2, btn2_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn2, start_x, 200);
    lv_obj_set_size(btn2, btn_width, btn_height);
    labelbtn = lv_label_create(btn2);
    lv_label_set_text(labelbtn, currencyTwo);
    lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
    lv_obj_center(labelbtn);

    start_x += btn_width + spacing;
  }

  if (currencyThree[0] != '\0') {
    btn3 = lv_btn_create(screen_main);
    lv_obj_add_style(btn3, &style_btn_default, 0);
    lv_obj_add_style(btn3, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn3, btn3_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn3, start_x, 200);
    lv_obj_set_size(btn3, btn_width, btn_height);
    labelbtn = lv_label_create(btn3);
    lv_label_set_text(labelbtn, currencyThree);
    lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
    lv_obj_center(labelbtn);
  }

  // Set initial currency (skip inhibit since acceptor is not enabled yet)
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

  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.pushPixels((uint16_t *)&color_p->full, w * h, true);
  lcd.endWrite();

  lv_disp_flush_ready(disp);
}

/*** Touchpad callback to read the touchpad ***/
void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
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
  return FundingService::pollBoltInvoice(http, sessionState);
}

/**
 * Sends a POST request to the GraphQL API endpoint with the provided Bolt
 * invoice. The request includes the necessary headers and payload to process
 * the payment.
 *
 * @param invoice The Bolt invoice to be sent as part of the request payload.
 * @return true if the payment was accepted (SUCCESS/PENDING/ALREADY_PAID),
 *         false on HTTP failure, GraphQL errors or FAILURE status.
 */
bool getBlinkLnURL(const char *invoice) {
  return FundingService::payInvoice(http, deviceState, invoice);
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
  float temp = ((total / 100.0) / fiatValue * 1e8);

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);

  if (chargeSelected > 0) {
    tempCharge = ((total / 100.0) / fiatValue * 1e8) * chargeSelected / 100;
    result = round(temp) - tempCharge;
  } else {
    result = round(temp);
  }

  Serial.print("Charge TEMP: ");
  Serial.println(tempCharge);
  Serial.print("Result (rounded satoshis): ");
  Serial.println(result);

  return FundingService::requestLnurlWithdraw(http, sessionState, result);
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
  Serial.print("Total (cents): ");
  Serial.println(total);
  Serial.print("EUR Value (price of 1 Bitcoin in euros): ");
  Serial.println(fiatValue);
  Serial.print("Charge: ");
  Serial.println(chargeSelected);

  float temp = ((total / 100.0) / fiatValue * 1e8);

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);

  if (chargeSelected > 0) {
    tempCharge = ((total / 100.0) / fiatValue * 1e8) * chargeSelected / 100;
    result = round(temp) - tempCharge;
  } else {
    result = round(temp);
  }

  Serial.print("Charge TEMP: ");
  Serial.println(tempCharge);
  Serial.print("Result (rounded satoshis): ");
  Serial.println(result);

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

  byte payload[51]; // 51 bytes is max one can get with xor-encryption

  size_t payload_len = xor_encrypt(
      payload, sizeof(payload), (uint8_t *)secretATM, strlen(secretATM), nonce,
      sizeof(nonce), randomPin, float(total));
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
  config.immediateStart = true;
  portal.join({elementsAux, saveAux, firstAux, savefirstAux, secondAux,
               savesecondAux, thirdAux, savethirdAux, guiAux, saveguiAux});
  portal.config(config);
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
        // Invoice received! Pay it and show the matching result screen
        Serial.println("Invoice received => processing payment");
        bool paymentOk = getBlinkLnURL(sessionState.boltInvoice);
        uiController.deleteQRCodeScreen();
        if (paymentOk) {
          createThankYouScreen();
          currentUiState = UI_THANK_YOU;
        } else {
          createPaymentErrorScreen();
          currentUiState = UI_PAYMENT_ERROR;
        }
        lv_task_handler();
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

  case UI_WAITING_FOR_TAP:
    // Non-blocking wait for tap after QR code (for LNbits)
    if (BTNA.wasPressed()) {
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

  case UI_THANK_YOU:
    // After thank you screen, wait then restart
    if (currentTime - stateEnterTime >= 1200) {
      Serial.println("Thank you timeout => restarting");
      ESP.restart();
    }
    break;

  case UI_PAYMENT_ERROR:
    // Keep the error visible long enough to be read/photographed, then restart
    if (currentTime - stateEnterTime >= 30000) {
      Serial.println("Payment error timeout => restarting");
      ESP.restart();
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
  lv_timer_handler();    // Let the GUI do its work
  portal.handleClient(); // Already non‑blocking

  // Handle UI state machine
  handleUiStateMachine();

  if (initialCheck) {
    previousMillis =
        millis() - interval; // So that it gets executed immediately after setup
    initialCheck = false;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    checkNetworkAndDeviceStatus();
    checkPrice();
    checkBalance();          // Check the balance every 5 minutes
    updateMainScreenLabel(); // Update the label on the main screen with the new
                             // balance
    lv_task_handler();
    // delay(5); // Removed to avoid blocking
  }

  // Check if user is inserting money
  int x = nonBlockingRead();

  if (x != -1) // Data available
  {
    for (int i = 0; i < billAmountIntOne.size();
         i++) // Using .size() method on std::vector
    {
      if ((i + 1) == x) {
        // A valid bill is detected
        bills = bills + billAmountIntOne[i];
        total = (coins + bills);
        if (!isInsertingMoney) {
          // Disable back button when money insertion starts
          /*if (btn_reset != nullptr) {
            lv_obj_add_state(btn_reset, LV_STATE_DISABLED);
            lv_task_handler();
          }*/

          createInsertMoneyScreen();
          lv_task_handler();
          isInsertingMoney = true;
          currentUiState = UI_INSERTING_MONEY;
          stateEnterTime = millis();
        }
        String lastBillString = "Last bill: " + String(billAmountIntOne[i]) +
                                " " + currencySelected;
        String totalString = "Total: " + String(total) + " " + currencySelected;
        String maxString = "MAX: " + String(maxamountSelected) + " " +
                           currencySelected + " from " +
                           deviceState.fundingSourceBuffer;

        lv_label_set_text(labelLastInserted, lastBillString.c_str());
        lv_label_set_text(labelTotalAmount, totalString.c_str());
        lv_label_set_text(labelMaxAmount, maxString.c_str());

        break; // Exit the for loop as we found a match
      }
    }
  }
  // Check button release or total (only if in INSERTING_MONEY state)
  if (currentUiState == UI_INSERTING_MONEY) {
    if ((BTNA.wasPressed() && total != 0) || total >= maxamountSelected) {
      // Process the total and reset variables for the next transaction.
      total = (coins + bills) * 100;

      Serial.print(F("Total: "));
      Serial.println(total);

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
        SerialPort1.write(185);
        digitalWrite(INHIBITMECH, LOW);
        Serial.print("Free heap (makeLNURL): ");
        Serial.println(ESP.getFreeHeap());
        lv_task_handler();
        Serial.println("lv_task_handler() - LNbits offline");
        currentUiState = UI_SHOWING_QR;
        stateEnterTime = millis();
        qrDebounceDone = false;
      } else {
        if (paymentService.isGaloy(deviceState.fundingSourceBuffer)) {
          uiController.deleteInsertMoneyScreen();
          Serial.println("deleteInsertMoneyScreen() - Blink online");
          const bool withdrawOk = createLNURLWithdraw();
          Serial.println("createLNURLWithdraw() - Blink online");
          // Turn off machines in both outcomes - cash is already inside
          SerialPort1.write(185);
          digitalWrite(INHIBITMECH, LOW);
          if (withdrawOk) {
            // Display the QR code for online
            showQRCodeLVGL(lnURLgen);
            Serial.println("showQRCodeLVGL() - Blink online");
            lv_task_handler();
            Serial.println("lv_task_handler() - Blink online");
            currentUiState = UI_SHOWING_QR;
            stateEnterTime = millis();
            qrDebounceDone = false;
            isBlinkFlow = true; // Mark that we're in Blink flow
          } else {
            // No LNURL/callback - showing a QR would trap the customer in a
            // polling loop that can never succeed.
            Serial.println("LNURL withdraw failed => payment error screen");
            createPaymentErrorScreen();
            lv_task_handler();
            currentUiState = UI_PAYMENT_ERROR;
            stateEnterTime = millis();
          }
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
            SerialPort1.write(185);
            digitalWrite(INHIBITMECH, LOW);
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
            SerialPort1.write(185);
            digitalWrite(INHIBITMECH, LOW);
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
