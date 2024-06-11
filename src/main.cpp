//= == == == == == == == == == == == == == == == == == == == == == == == == == == == = //
//============EDIT IF USING DIFFERENT HARDWARE============//
//========================================================//
// v0.1

bool format = false; // true for formatting FOSSA memory, use once, then make false and reflash

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
// #define LV_CONF_INCLUDE_SIMPLE

/* Uncomment below line to draw on screen with touch */
// #define DRAW_ON_SCREEN

#include <LovyanGFX.hpp> // main library
static LGFX lcd;         // declare display variable

#include <lvgl.h>
#include "lv_conf.h"
#include "lv_font_montserrat_bold_60.c"
#include "lv_font_the_bold_48.c"
lv_color_t colors[] = {
    LV_COLOR_PURPLE,
    LV_COLOR_RED,
    LV_COLOR_ORANGE,
    LV_COLOR_YELLOW,
    LV_COLOR_GREEN,
    LV_COLOR_BLUE};

#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Wire.h>
using WebServerClass = WebServer;
fs::SPIFFSFS &FlashFS = SPIFFS;
#define FORMAT_ON_FAIL true

#include <AutoConnect.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <JC_Button.h>

#include <Hash.h>
#include <Bitcoin.h>
#include <HTTPClient.h>

#include <vector>
#include <iostream>

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

std::vector<int> billAmountIntOne;
std::vector<int> billAmountIntTwo;
std::vector<int> billAmountIntThree;

size_t originalSizeOne = 0;
size_t originalSizeTwo = 0;
size_t originalSizeThree = 0;

// start old
HTTPClient http; // Declare object of class HTTPClient

#define PARAM_FILE "/elements.json"
#define FIRST_FILE "/first.json"
#define SECOND_FILE "/second.json"
#define THIRD_FILE "/third.json"

String qrData;
String password = "vojdivon"; // default WiFi AP password
String lnurl;
String lnurl2;
String lnurl3;
String baseURLATM1;
String baseURLATM = baseURLATM1;
String baseURLATM2;
String baseURLATM3;
String secretATM1;
String secretATM = secretATM1;
String secretATM2;
String secretATM3;
String currencyATM;
String currencyATM2;
String currencyATM3;
String currencyone = "EUR";
String currencytwo;
String currencythree;
String currencySelected = currencyone;

lv_obj_t *btn1;
lv_obj_t *btn2;
lv_obj_t *btn3;

lv_obj_t *burnTextLabel; // This will hold the reference to the label created in burn_text_demo

char localSSIDBuffer[100] = {0};
char localPassBuffer[100] = {0};

const char *localssid = localSSIDBuffer;
const char *localpass = localPassBuffer;

lv_obj_t *btn_wifi;
lv_style_t style_connected;
lv_style_t style_disconnected;

String atmtitle = "FIAT HELL";
String atmsubtitle;
String atmdesc = "";
String pincode = "1111";
String blinkapikey;
String blinkwalletid;

String lnbitsURL;
String adminkey;
String readkey;

String lnURLgen;
String modifiedLnURLgen;
String callback;
String boltInvoice;
String totalStr;

String paymentRequest;
String payload;

static char fundingSourceBuffer[100] = {0}; // Ensure this buffer is large enough for possible values
const char *fundingsource = fundingSourceBuffer;
static char enableSwitchBuffer[100] = {0}; // Ensure this buffer is large enough for possible values
const char *enableswitch = enableSwitchBuffer;

int bills;
float coins;
float total;
float maxamount;
float maxamountSelected = maxamount;
float maxamount2;
float maxamount3;
int charge1;
int charge2;
int charge3;
int chargeSelected = charge1;
float fiatBalance; // zoztatok v €
float fiatValue = 0;
float tempCharge;
long result;
char buffer[32];
lv_obj_t *balanceValueLabel = nullptr; // Make this global so you can update it later
lv_obj_t *fiatValueLabel = nullptr;    // initialize globally
lv_obj_t *chargeValueLabel = nullptr;  // initialize globally
bool isInsertingMoney = false;

bool billBool = true;
// bool coinBool = false;

int moneyTimer = 0;

const long interval = 300000; // 5 minutes in milliseconds
unsigned long previousMillis = 0;
long balanceSats = 0; // Assuming it's a long or an appropriate type
bool initialCheck = true;

const char *graphqlEndpoint = "https://api.blink.sv/graphql";
const char *primaryApiEndpoint = "https://api.lnbc.sk/v1/lnurl";
const char *secondaryApiEndpoint = "https://api.lnurlproxy.me/v1/lnurl";
const char *primaryConversionAPI = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=";
const char *secondaryConversionAPI = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=";

// float coinAmountFloat[6] = {0.05, 0.1, 0.2, 0.5, 1, 2};
//  int billAmountSize = sizeof(billAmountIntOne) / sizeof(int);
// float coinAmountSize = sizeof(coinAmountFloat) / sizeof(float);

HardwareSerial SerialPort1(1);
HardwareSerial SerialPort2(2);

// lv_timer_t *escrow_task = NULL; // Declare this at global scope so you can delete it later

Button BTNA(BTN1);

lv_obj_t *screen_logo, *screen_portal, *screen_api, *screen_thx, *screen_main, *screen_insert_money, *screen_qr, *screen_settings;
lv_obj_t *labelbtn;
lv_obj_t *fiathell;
lv_obj_t *labelLastInserted = nullptr;
lv_obj_t *labelTotalAmount = nullptr;
lv_obj_t *labelMaxAmount = nullptr;

// Switch fundingsource
lv_obj_t *switch_label;
lv_obj_t *switch_fund;
lv_obj_t *img_blink;
lv_obj_t *img_lnbits;

/* ----------------------------------
-------------- PORTAL ---------------
-----------------------------------*/

bool triggerAp = false;

String content = "<h1>ATM Access-point</br>For easy variable setting</h1>";

#include "pageone.h"
#include "pagefirst.h"
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
// end old

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
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
void createLogoScreen();
void createPortalScreen();
void createAPIScreen();
void createMainScreen();
void createSwitch();
void escrow_and_balance_task(lv_timer_t *t);
void createInsertMoneyScreen();
void lv_button_currency();
void updateBurnText();
void updateMainScreenLabel();
void checkPrice();
void checkBalance();
void checkSerialPort1();
bool isBlink();
bool isLNbits();
bool wifiStatus();
void showQRCodeLVGL(const char *data);
int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen, uint8_t *nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents);
void deleteMainScreen();
void deleteInsertMoneyScreen();
void displaySettingsScreen();
void checkNetworkAndDeviceStatus();
void btn_config_portal_event_handler(lv_event_t *e);
void startConfigPortal();
void btn_back_event_handler(lv_event_t *e);
void switchFundingSource();
void update_settings_button_style();
void settings_btn_event_cb(lv_event_t *e);
void createResetButton(lv_obj_t *parent);
void createBackButton(lv_obj_t *parent);
static void reset_btn_event_cb(lv_event_t *e);
void setupPinEntryComponents(lv_obj_t *parent);
void printHeapStatus();

/**
 * @brief A class representing a sequence of characters.
 *
 * The String class provides methods for manipulating and accessing strings.
 * It is commonly used in C++ to handle text data.
 */
String
getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  const int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void to_upper(char *arr)
{
  for (size_t i = 0; i < strlen(arr); i++)
  {
    if (arr[i] >= 'a' && arr[i] <= 'z')
    {
      arr[i] = arr[i] - 'a' + 'A';
    }
  }
}

void setup()
{
  lcd.init(); // Initialize LovyanGFX
  lv_init();  // Initialize lvgl

  // Setting display to landscape
  if (lcd.width() < lcd.height())
    lcd.setRotation(lcd.getRotation() ^ 1);

  /* LVGL : Setting up buffer to use for display */
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  /*** LVGL : Setup & Initialize the display device driver ***/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*** LVGL : Setup & Initialize the input device driver ***/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  createLogoScreen();
  lv_task_handler(); // Process LVGL tasks to refresh the screen.

  BTNA.begin();

  Serial.begin(115200);
  delay(10);
  WiFiClientSecure client;

  int timer = 0;
  while (timer < 2000)
  {
    lv_task_handler();
    BTNA.read();
    if (BTNA.wasReleased())
    {
      timer = 5000;
      triggerAp = true;
    }
    timer = timer + 100;
    delay(100);
  }

  SerialPort1.begin(300, SERIAL_8N2, TX1, RX1);
  SerialPort2.begin(4800, SERIAL_8N1, TX2);

  pinMode(INHIBITMECH, OUTPUT);

  FlashFS.begin(FORMAT_ON_FAIL);
  SPIFFS.begin(true);
  if (format == true)
  {
    SPIFFS.format();
  }
  // get the saved details and store in global variables
  File paramFile = FlashFS.open(PARAM_FILE, "r");
  if (paramFile)
  {
    StaticJsonDocument<2500> conf;
    DeserializationError error = deserializeJson(conf, paramFile.readString());

    const JsonObject conf0 = conf[0];
    const char *conf0Char = conf0["value"];
    password = conf0Char;

    const JsonObject conf1 = conf[1];
    if (conf1["value"] != nullptr)
    {
      strlcpy(localSSIDBuffer, conf1["value"], sizeof(localSSIDBuffer));
      localssid = localSSIDBuffer;
    }

    const JsonObject conf2 = conf[2];
    if (conf2["value"] != nullptr)
    {
      strlcpy(localPassBuffer, conf2["value"], sizeof(localPassBuffer));
      localpass = localPassBuffer;
    }

    const JsonObject conf3 = conf[3];
    const char *conf3Char = conf3["value"];
    atmdesc = conf3Char;

    const JsonObject conf4 = conf[4];
    const char *conf4Char = conf4["value"];
    atmsubtitle = conf4Char;

    const JsonObject conf5 = conf[5];
    const char *conf5Char = conf5["value"];
    atmtitle = conf5Char;

    const JsonObject conf6 = conf[6];
    const char *conf6Char = conf6["value"];
    pincode = conf6Char;

    const JsonObject conf7 = conf[7];
    const char *conf7Char = conf7["name"];
    JsonArray valuesEnableSwitch = conf7["value"];
    int checkedIndex = conf7["checked"];

    if (checkedIndex > 0 && checkedIndex <= valuesEnableSwitch.size())
    {
      // Copy the selected funding source to the global buffer
      strlcpy(enableSwitchBuffer, valuesEnableSwitch[checkedIndex - 1], sizeof(enableSwitchBuffer));
      enableswitch = enableSwitchBuffer; // Point fundingsource to the global buffer

      Serial.print(conf7Char);
      Serial.print(": ");
      Serial.println(enableswitch);
    }
  }
  else
  {
    triggerAp = true;
  }
  paramFile.close();
  server.on("/", []()
            {
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content); });

  elementsAux.load(FPSTR(PAGE_ELEMENTS));
  elementsAux.on([](AutoConnectAux &aux, PageArgument &arg)
                 {
    File param = FlashFS.open(PARAM_FILE, "r");
    if (param)
    {
      aux.loadElement(param, {"password", "localssid", "localpass", "atmdesc", "atmsubtitle", "atmtitle", "pincode", "enableswitch"});
      param.close();
    }

    if (portal.where() == "/config")
    {
      File param = FlashFS.open(PARAM_FILE, "r");
      if (param)
      {
        aux.loadElement(param, {"password", "localssid", "localpass", "atmdesc", "atmsubtitle", "atmtitle", "pincode", "enableswitch"});
        param.close();
      }
    }
    return String(); });

  // First page start
  //  get the saved details and store in global variables
  File firstFile = FlashFS.open(FIRST_FILE, "r");
  if (firstFile)
  {
    StaticJsonDocument<2500> docFirst;
    DeserializationError error = deserializeJson(docFirst, firstFile.readString());

    const JsonObject docFirst0 = docFirst[0];
    const char *docFirst0Char = docFirst0["value"];
    blinkapikey = docFirst0Char;

    const JsonObject docFirst1 = docFirst[1];
    const char *docFirst1Char = docFirst1["value"];
    blinkwalletid = docFirst1Char;

    const JsonObject docFirst2 = docFirst[2];
    const char *docFirst2Char = docFirst2["value"];
    const String lnurlATM = docFirst2Char;
    baseURLATM1 = getValue(lnurlATM, ',', 0);
    secretATM1 = getValue(lnurlATM, ',', 1);
    currencyATM = getValue(lnurlATM, ',', 2);

    const JsonObject docFirst3 = docFirst[3];
    const char *docFirst3Char = docFirst3["value"];
    adminkey = docFirst3Char;

    const JsonObject docFirst4 = docFirst[4];
    const char *docFirst4Char = docFirst4["value"];
    readkey = docFirst4Char;

    const JsonObject docFirst5 = docFirst[5];
    const char *docFirst5Char = docFirst5["value"];
    currencyone = docFirst5Char;

    const JsonObject docFirst6 = docFirst[6];
    const char *docFirst6Char = docFirst6["value"];
    const String billmech = docFirst6Char;
    if (billmech != "")
    {
      int startPos = 0;
      int commaPos = billmech.indexOf(',', startPos);

      while (commaPos != -1)
      {
        String value = billmech.substring(startPos, commaPos);
        billAmountIntOne.push_back(value.toInt());

        startPos = commaPos + 1;
        commaPos = billmech.indexOf(',', startPos);
      }

      // Don't forget the last value after the last comma
      String value = billmech.substring(startPos);
      billAmountIntOne.push_back(value.toInt());
    }

    const JsonObject docFirst7 = docFirst[7];
    const char *docFirst7Char = docFirst7["value"];
    const String maxamountstr = docFirst7Char;
    maxamount = maxamountstr.toInt();

    const JsonObject docFirst8 = docFirst[8];
    const char *docFirst8Char = docFirst8["value"];
    const String chargestr = docFirst8Char;
    charge1 = chargestr.toInt();

    const JsonObject docFirst9 = docFirst[9];
    const char *docFirst9Char = docFirst9["name"];
    JsonArray valuesFundingSource = docFirst9["value"];
    int checkedIndex = docFirst9["checked"];

    if (checkedIndex > 0 && checkedIndex <= valuesFundingSource.size())
    {
      // Copy the selected funding source to the global buffer
      strlcpy(fundingSourceBuffer, valuesFundingSource[checkedIndex - 1], sizeof(fundingSourceBuffer));
      fundingsource = fundingSourceBuffer; // Point fundingsource to the global buffer

      Serial.print(docFirst9Char);
      Serial.print(": ");
      Serial.println(fundingSourceBuffer);
    }
  }
  else
  {
    triggerAp = true;
  }
  firstFile.close();
  server.on("/", []()
            {
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content); });

  firstAux.load(FPSTR(PAGE_FIRST));
  firstAux.on([](AutoConnectAux &aux, PageArgument &arg)
              {
    File paramFirst = FlashFS.open(FIRST_FILE, "r");
    if (paramFirst)
    {
      aux.loadElement(paramFirst, {"blinkapikey", "blinkwalletid", "lnurl", "adminkey", "readkey", "currencyone", "billmech", "maxamount", "charge1", "fundingsource"});
      paramFirst.close();
    }

    if (portal.where() == "/first")
    {
      File paramFirst = FlashFS.open(FIRST_FILE, "r");
      if (paramFirst)
      {
        aux.loadElement(paramFirst, {"blinkapikey", "blinkwalletid", "lnurl", "adminkey", "readkey", "currencyone", "billmech", "maxamount", "charge1", "fundingsource"});
        paramFirst.close();
      }
    }
    return String(); });

  // Second page start
  //  get the saved details and store in global variables
  File secondFile = FlashFS.open(SECOND_FILE, "r");
  if (secondFile)
  {
    StaticJsonDocument<2500> docSecond;
    DeserializationError error = deserializeJson(docSecond, secondFile.readString());

    const JsonObject docSecond0 = docSecond[0];
    const char *docSecond0Char = docSecond0["value"];
    currencytwo = docSecond0Char;

    const JsonObject docSecond1 = docSecond[1];
    const char *docSecond1Char = docSecond1["value"];
    const String lnurlATM2 = docSecond1Char;
    baseURLATM2 = getValue(lnurlATM2, ',', 0);
    secretATM2 = getValue(lnurlATM2, ',', 1);
    currencyATM2 = getValue(lnurlATM2, ',', 2);

    const JsonObject docSecond2 = docSecond[2];
    const char *docSecond2Char = docSecond2["value"];
    const String billmech2 = docSecond2Char;

    if (billmech2 != "")
    {
      int startPos = 0;
      int commaPos = billmech2.indexOf(',', startPos);

      while (commaPos != -1)
      {
        String value = billmech2.substring(startPos, commaPos);
        billAmountIntTwo.push_back(value.toInt());

        startPos = commaPos + 1;
        commaPos = billmech2.indexOf(',', startPos);
      }

      // Don't forget the last value after the last comma
      String value = billmech2.substring(startPos);
      billAmountIntTwo.push_back(value.toInt());
    }

    const JsonObject docSecond3 = docSecond[3];
    const char *docSecond3Char = docSecond3["value"];
    const String maxamountstrtwo = docSecond3Char;
    maxamount2 = maxamountstrtwo.toInt();

    const JsonObject docSecond4 = docSecond[4];
    const char *docSecond4Char = docSecond4["value"];
    const String chargestrtwo = docSecond4Char;
    charge2 = chargestrtwo.toInt();
  }
  else
  {
    triggerAp = true;
  }
  secondFile.close();
  server.on("/", []()
            {
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content); });

  secondAux.load(FPSTR(PAGE_SECOND));
  secondAux.on([](AutoConnectAux &aux, PageArgument &arg)
               {
    File paramSecond = FlashFS.open(SECOND_FILE, "r");
    if (paramSecond)
    {
      aux.loadElement(paramSecond, {"currencytwo", "lnurl2", "billmech2", "maxamount2", "charge2"});
      paramSecond.close();
    }

    if (portal.where() == "/second")
    {
      File paramSecond = FlashFS.open(SECOND_FILE, "r");
      if (paramSecond)
      {
        aux.loadElement(paramSecond, {"currencytwo", "lnurl2", "billmech2", "maxamount2", "charge2"});
        paramSecond.close();
      }
    }
    return String(); });

  //*
  //*
  //*
  // get the saved details and store in global variables
  File thirdFile = FlashFS.open(THIRD_FILE, "r");
  if (thirdFile)
  {
    StaticJsonDocument<2500> docThird;
    DeserializationError error = deserializeJson(docThird, thirdFile.readString());

    const JsonObject docThird0 = docThird[0];
    const char *docThird0Char = docThird0["value"];
    currencythree = docThird0Char;

    const JsonObject docThird1 = docThird[1];
    const char *docThird1Char = docThird1["value"];
    const String lnurlATM3 = docThird1Char;
    baseURLATM3 = getValue(lnurlATM3, ',', 0);
    secretATM3 = getValue(lnurlATM3, ',', 1);
    currencyATM3 = getValue(lnurlATM3, ',', 2);

    const JsonObject docThird2 = docThird[2];
    const char *docThird2Char = docThird2["value"];
    const String billmech3 = docThird2Char;

    if (billmech3 != "")
    {
      int startPos = 0;
      int commaPos = billmech3.indexOf(',', startPos);

      while (commaPos != -1)
      {
        String value = billmech3.substring(startPos, commaPos);
        billAmountIntThree.push_back(value.toInt());

        startPos = commaPos + 1;
        commaPos = billmech3.indexOf(',', startPos);
      }

      // Don't forget the last value after the last comma
      String value = billmech3.substring(startPos);
      billAmountIntThree.push_back(value.toInt());
    }

    const JsonObject docThird3 = docThird[3];
    const char *docThird3Char = docThird3["value"];
    const String maxamountstrthree = docThird3Char;
    maxamount3 = maxamountstrthree.toInt();

    const JsonObject docThird4 = docThird[4];
    const char *docThird4Char = docThird4["value"];
    const String chargestrthree = docThird4Char;
    charge3 = chargestrthree.toInt();
  }
  else
  {
    triggerAp = true;
  }
  thirdFile.close();
  server.on("/", []()
            {
    content += AUTOCONNECT_LINK(COG_24);
    server.send(200, "text/html", content); });

  thirdAux.load(FPSTR(PAGE_THIRD));
  thirdAux.on([](AutoConnectAux &aux, PageArgument &arg)
              {
    File paramThird = FlashFS.open(THIRD_FILE, "r");
    if (paramThird)
    {
      aux.loadElement(paramThird, {"currencythree", "lnurl3", "billmech3", "maxamount3", "charge3"});
      paramThird.close();
    }

    if (portal.where() == "/third")
    {
      File paramThird = FlashFS.open(THIRD_FILE, "r");
      if (paramThird)
      {
        aux.loadElement(paramThird, {"currencythree", "lnurl3", "billmech3", "maxamount3", "charge3"});
        paramThird.close();
      }
    }
    return String(); });

  //*
  //*
  //*
  // Save page one
  saveAux.load(FPSTR(PAGE_SAVE));
  saveAux.on([](AutoConnectAux &aux, PageArgument &arg)
             {
    aux["caption"].value = PARAM_FILE;
    File param = FlashFS.open(PARAM_FILE, "w");
    if (param)
    {
      // save as a loadable set for parameters.
      elementsAux.saveElement(param, {"password", "localssid", "localpass", "atmdesc", "atmsubtitle", "atmtitle", "pincode", "enableswitch"});
      param.close();
      // read the saved elements again to display.
      param = FlashFS.open(PARAM_FILE, "r");
      aux["echo"].value = param.readString();
      param.close();
    }
    else
    {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String(); });

  // Save first page
  savefirstAux.load(FPSTR(FIRST_SAVE));
  savefirstAux.on([](AutoConnectAux &aux, PageArgument &arg)
                  {
    aux["caption"].value = FIRST_FILE;
    File paramFirst = FlashFS.open(FIRST_FILE, "w");
    if (paramFirst)
    {
      // save as a loadable set for parameters.
      firstAux.saveElement(paramFirst, {"blinkapikey", "blinkwalletid", "lnurl", "adminkey", "readkey", "currencyone", "billmech", "maxamount", "charge1", "fundingsource"});
      paramFirst.close();
      // read the saved elements again to display.
      paramFirst = FlashFS.open(FIRST_FILE, "r");
      aux["echo"].value = paramFirst.readString();
      paramFirst.close();
    }
    else
    {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String(); });

  // Save second page
  savesecondAux.load(FPSTR(SECOND_SAVE));
  savesecondAux.on([](AutoConnectAux &aux, PageArgument &arg)
                   {
    aux["caption"].value = SECOND_FILE;
    File paramSecond = FlashFS.open(SECOND_FILE, "w");
    if (paramSecond)
    {
      // save as a loadable set for parameters.
      secondAux.saveElement(paramSecond, {"currencytwo", "lnurl2", "billmech2", "maxamount2", "charge2"});
      paramSecond.close();
      // read the saved elements again to display.
      paramSecond = FlashFS.open(SECOND_FILE, "r");
      aux["echo"].value = paramSecond.readString();
      paramSecond.close();
    }
    else
    {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String(); });
  // Save third page
  savethirdAux.load(FPSTR(THIRD_SAVE));
  savethirdAux.on([](AutoConnectAux &aux, PageArgument &arg)
                  {
    aux["caption"].value = THIRD_FILE;
    File paramThird = FlashFS.open(THIRD_FILE, "w");
    if (paramThird)
    {
      // save as a loadable set for parameters.
      thirdAux.saveElement(paramThird, {"currencythree", "lnurl3", "billmech3", "maxamount3", "charge3"});
      paramThird.close();
      // read the saved elements again to display.
      paramThird = FlashFS.open(THIRD_FILE, "r");
      aux["echo"].value = paramThird.readString();
      paramThird.close();
    }
    else
    {
      aux["echo"].value = "Filesystem failed to open.";
    }
    return String(); });

  originalSizeOne = billAmountIntOne.size();
  originalSizeTwo = billAmountIntTwo.size();
  originalSizeThree = billAmountIntThree.size();

  // First merge billAmountIntOne and billAmountIntTwo
  if (currencyATM2 != "" || currencytwo != "")
  {
    billAmountIntOne.insert(billAmountIntOne.end(), billAmountIntTwo.begin(), billAmountIntTwo.end());
  }
  // Check if currencyATM3 is not empty
  if (currencyATM3 != "" || currencythree != "")
  {
    // Then merge billAmountIntThree into the now-extended billAmountIntOne
    billAmountIntOne.insert(billAmountIntOne.end(), billAmountIntThree.begin(), billAmountIntThree.end());
  }

  config.auth = AC_AUTH_BASIC;
  config.authScope = AC_AUTHSCOPE_AUX;
  config.ticker = true;
  config.autoReconnect = true;
  config.apid = "LN ATM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  config.psk = password;
  config.menuItems = AC_MENUITEM_CONFIGNEW | AC_MENUITEM_OPENSSIDS | AC_MENUITEM_DEVINFO | AC_MENUITEM_RESET;
  config.title = "LN ATM";
  config.reconnectInterval = 1;
  // config.username = "user";
  // config.password = "password";

  WiFi.begin(localssid, localpass);
  checkPrice();
  checkBalance();
  // updateMainScreenLabel();

  Serial.print(F("APP PASSWORD: "));
  Serial.println(password);
  Serial.print(F("SSID: "));
  Serial.println(localssid);
  Serial.print(F("PASSWORD: "));
  Serial.println(localpass);
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
  Serial.println(enableswitch);
  Serial.print(F("Currency selected: "));
  Serial.println(currencySelected);
  Serial.print(F("Main currency: "));
  Serial.println(currencyone);
  Serial.print(F("Second currency: "));
  Serial.println(currencytwo);
  Serial.print(F("Third currency: "));
  Serial.println(currencythree);
  Serial.print(F("ATM currency: "));
  Serial.println(currencyATM);
  Serial.print(F("ATM2 currency: "));
  Serial.println(currencyATM2);
  Serial.print(F("ATM3 currency: "));
  Serial.println(currencyATM3);
  Serial.print(F("MAX (selected): "));
  Serial.println(maxamountSelected);
  Serial.print(F("Charge: "));
  Serial.println(charge1);

  if (triggerAp == true)
  {
    createPortalScreen();
    lv_task_handler(); // Process LVGL tasks to refresh the screen.
    digitalWrite(11, LOW);
    // lv_obj_clean(lv_scr_act()); // Clear the entire screen

    Serial.print(F("Entered Config Portal"));

    startConfigPortal();
    timer = 2000;
  }
  else if ((fundingSourceBuffer == "LNbits" && (currencyATM == "" || adminkey == "" || readkey == "")) || (fundingSourceBuffer == "Blink" && (blinkapikey == "" || blinkwalletid == "") || currencyone == ""))
  {
    createAPIScreen();
    lv_task_handler(); // Process LVGL tasks to refresh the screen.
    digitalWrite(11, LOW);
    // lv_obj_clean(lv_scr_act()); // Clear the entire screen

    Serial.print(F("Entered Config Portal"));

    startConfigPortal();
    timer = 2000;
  }
  else
  {
    createMainScreen();
  }

  // Extract "https://your.lnbits.com" from baseURLATM "https://your.lnbits.com/lnurldevice/api/v1/lnurl/<id>";
  int thirdSlash = 0;
  int count = 0;

  for (int i = 0; i < baseURLATM.length(); i++)
  {
    if (baseURLATM.charAt(i) == '/')
    {
      count++;
      if (count == 3)
      {
        thirdSlash = i;
        break;
      }
    }
  }

  lnbitsURL = baseURLATM.substring(0, thirdSlash);
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
int nonBlockingRead()
{
  if (SerialPort1.available())
  {
    return SerialPort1.read();
  }
  return -1; // No data available
}

// Create the logo screen
/**
 * @brief Sets the angle of an LVGL arc object.
 *
 * This function is used to set the angle of an LVGL arc object.
 *
 * @param obj Pointer to the LVGL arc object.
 * @param v The angle value to set.
 */
static void set_angle(void *obj, int32_t v)
{
  lv_arc_set_value((lv_obj_t *)obj, v);
}

/**
 * Checks the status of the WiFi connection.
 *
 * @return true if the WiFi is connected, false otherwise.
 */
bool wifiStatus()
{
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Creates a logo screen with a logo, URL label, arc animation, and an image.
 *
 * This function creates a new screen and adds various graphical elements to it, including a logo,
 * a URL label, an arc animation, and an image. The logo screen is then loaded and displayed.
 *
 * @note The function assumes that the necessary resources (e.g., fonts, images) have been properly
 *       initialized and loaded beforehand.
 */
void createLogoScreen()
{
  screen_logo = lv_obj_create(NULL); // Create a new screen

  // Put your logo creation code here, but replace `lv_scr_act()` with `screen_logo`
  String LVGL_ATMURL = "ATM.LNPAY.EU";
  lv_obj_t *atmurl = lv_label_create(screen_logo); // use screen_logo as the parent
  lv_label_set_text(atmurl, LVGL_ATMURL.c_str());
  lv_obj_align(atmurl, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_text_font(atmurl, &lv_font_montserrat_28, 0); // Set font (replace with appropriate font)
  lv_obj_set_style_text_color(atmurl, LV_COLOR_PURPLE, 0);

  /*Create an Arc*/
  lv_obj_t *arc = lv_arc_create(screen_logo); // Create the arc on screen_logo
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);  /*Be sure the knob is not displayed*/
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/
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
  lv_img_set_src(img1, &btcSmallImg);          // Set the image source to your converted image (my_image)
  lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);   // Align the image to the center of the screen

  lv_scr_load(screen_logo);
}

// Create the portal screen
/**
 * @brief Creates the portal screen.
 *
 * This function creates a new screen and adds various labels to display instructions for connecting to a Wi-Fi network.
 *
 * @note The function assumes that the necessary fonts have been loaded and the screen_portal object has been declared globally.
 *
 * @note The labels are aligned vertically and centered horizontally on the screen.
 *
 * @note The text for the labels is set using predefined string constants.
 *
 * @note The font styles for the labels are set using predefined font objects.
 *
 * @note The screen_portal object is loaded as the active screen.
 */
void createPortalScreen()
{
  screen_portal = lv_obj_create(NULL); // Create a new screen

  String LVGL_PORTAL_ON = "Config launched";
  lv_obj_t *portalon = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(portalon, LVGL_PORTAL_ON.c_str());             // set label text
  lv_obj_align(portalon, LV_ALIGN_TOP_MID, 0, 20);                 // Center but 20 from the top
  lv_obj_set_style_text_font(portalon, &lv_font_montserrat_48, 0); // Use the large font
  lv_obj_set_style_text_color(portalon, LV_COLOR_WHITE, 0);

  String LVGL_CONNECT_TO_WIFI = "Connect with your phone via Wi-Fi.";
  lv_obj_t *connecttowifi = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(connecttowifi, LVGL_CONNECT_TO_WIFI.c_str());       // set label text
  lv_obj_align(connecttowifi, LV_ALIGN_TOP_MID, 0, 80);                 // Center but 20 from the top
  lv_obj_set_style_text_font(connecttowifi, &lv_font_montserrat_24, 0); // Use the large font
  // lv_obj_set_style_text_color(atmurl, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_ONE = "Find new Wi-Fi network 'LN ATM-xxxx' ";
  lv_obj_t *portaltextone = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(portaltextone, LVGL_PORTAL_TEXT_ONE.c_str());       // set label text
  lv_obj_align(portaltextone, LV_ALIGN_TOP_MID, 0, 120);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextone, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_TWO = "in your phone and connect to it. After ";
  lv_obj_t *portaltexttwo = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(portaltexttwo, LVGL_PORTAL_TEXT_TWO.c_str());       // set label text
  lv_obj_align(portaltexttwo, LV_ALIGN_TOP_MID, 0, 160);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltexttwo, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_THREE = "you are connected, open ATM settings ";
  lv_obj_t *portaltextthree = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(portaltextthree, LVGL_PORTAL_TEXT_THREE.c_str());     // set label text
  lv_obj_align(portaltextthree, LV_ALIGN_TOP_MID, 0, 200);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextthree, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_FOUR = "and set your preferences";
  lv_obj_t *portaltextfour = lv_label_create(screen_portal);             // full screen as the parent
  lv_label_set_text(portaltextfour, LVGL_PORTAL_TEXT_FOUR.c_str());      // set label text
  lv_obj_align(portaltextfour, LV_ALIGN_TOP_MID, 0, 240);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextfour, &lv_font_montserrat_22, 0); // Use the large font

  lv_scr_load(screen_portal);
}

/**
 * @brief Creates the API screen.
 *
 * This function creates a new screen and adds various labels to display API information.
 * The labels include the API title, restart instructions, connection instructions, and preference instructions.
 *
 * @note The API data is currently set to "API DATA MISSING".
 *
 * @note The labels are aligned and styled using different fonts.
 *
 * @note The screen is loaded after all the labels are created.
 */
void createAPIScreen()
{
  screen_api = lv_obj_create(NULL); // Create a new screen

  String LVGL_API = "API DATA MISSING";
  lv_obj_t *apititle = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(apititle, LVGL_API.c_str());                   // set label text
  lv_obj_align(apititle, LV_ALIGN_TOP_MID, 0, 20);                 // Center but 20 from the top
  lv_obj_set_style_text_font(apititle, &lv_font_montserrat_48, 0); // Use the large font
  lv_obj_set_style_text_color(apititle, LV_COLOR_WHITE, 0);

  String LVGL_CONNECT_TO_WIFI = "Connect with your phone via Wi-Fi.";
  lv_obj_t *connecttowifi = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(connecttowifi, LVGL_CONNECT_TO_WIFI.c_str());       // set label text
  lv_obj_align(connecttowifi, LV_ALIGN_TOP_MID, 0, 80);                 // Center but 20 from the top
  lv_obj_set_style_text_font(connecttowifi, &lv_font_montserrat_24, 0); // Use the large font
  // lv_obj_set_style_text_color(atmurl, LV_COLOR_WHITE, 0);

  String LVGL_PORTAL_TEXT_ONE = "Find new Wi-Fi network 'LN ATM-xxxx' ";
  lv_obj_t *portaltextone = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(portaltextone, LVGL_PORTAL_TEXT_ONE.c_str());       // set label text
  lv_obj_align(portaltextone, LV_ALIGN_TOP_MID, 0, 120);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextone, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_TWO = "in your phone and connect to it. After ";
  lv_obj_t *portaltexttwo = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(portaltexttwo, LVGL_PORTAL_TEXT_TWO.c_str());       // set label text
  lv_obj_align(portaltexttwo, LV_ALIGN_TOP_MID, 0, 160);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltexttwo, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_THREE = "you are connected, open ATM settings ";
  lv_obj_t *portaltextthree = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(portaltextthree, LVGL_PORTAL_TEXT_THREE.c_str());     // set label text
  lv_obj_align(portaltextthree, LV_ALIGN_TOP_MID, 0, 200);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextthree, &lv_font_montserrat_22, 0); // Use the large font

  String LVGL_PORTAL_TEXT_FOUR = "and set your preferences";
  lv_obj_t *portaltextfour = lv_label_create(screen_api);                // full screen as the parent
  lv_label_set_text(portaltextfour, LVGL_PORTAL_TEXT_FOUR.c_str());      // set label text
  lv_obj_align(portaltextfour, LV_ALIGN_TOP_MID, 0, 240);                // Center but 20 from the top
  lv_obj_set_style_text_font(portaltextfour, &lv_font_montserrat_22, 0); // Use the large font

  lv_scr_load(screen_api);
}

/*PIN*/
static lv_obj_t *pin_code_label;
static char pin_code[5] = ""; // Assuming a 4-digit PIN

// Function to handle button clicks
static void btn_event_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *label = lv_obj_get_child(btn, 0); // Get the label from the button
  const char *txt = lv_label_get_text(label); // Get the text from the label

  if (strlen(pin_code) < 4)
  {
    strcat(pin_code, txt);
    lv_label_set_text_fmt(pin_code_label, "PIN: %s", pin_code);
  }

  // Check if PIN is complete and correct
  if (strlen(pin_code) == 4)
  {
    if (strcmp(pin_code, pincode.c_str()) == 0)
    { // Assuming '1234' is the correct PIN
      // Trigger access to the settings screen
      displaySettingsScreen();
    }
    else
    {
      lv_label_set_text(pin_code_label, "PIN: Incorrect");
      memset(pin_code, 0, sizeof(pin_code)); // Reset the pin_code for a new attempt
    }
  }
}

void createPinEntryScreen()
{
  memset(pin_code, 0, sizeof(pin_code));      // Reset the pin_code every time screen is created
  lv_obj_t *pin_screen = lv_obj_create(NULL); // Create a new screen
  lv_scr_load(pin_screen);                    // Load the new screen
  setupPinEntryComponents(pin_screen);        // Setup pin entry components
  createBackButton(pin_screen);               // Create a back button
  Serial.println("PIN entry screen created");
  Serial.print("PIN loaded: ");
  Serial.println(pincode);
}

void setupPinEntryComponents(lv_obj_t *parent)
{
  const int btn_width = 50;
  const int btn_height = 50;
  const int btn_spacing = 10;
  const int cols = 3;
  int rows = (10 + cols - 1) / cols; // Calculate number of rows needed

  // Get parent (screen) dimensions
  lv_coord_t parent_width = lv_obj_get_width(parent);
  lv_coord_t parent_height = lv_obj_get_height(parent);

  // Calculate total keypad width and height
  int keypad_width = cols * btn_width + (cols - 1) * btn_spacing;
  int keypad_height = rows * btn_height + (rows - 1) * btn_spacing;

  // Calculate the starting position to center the keypad
  int start_x = (parent_width - keypad_width) / 2;
  int start_y = (parent_height - keypad_height) / 2;

  // Create PIN code label
  pin_code_label = lv_label_create(parent);
  lv_label_set_text(pin_code_label, "PIN: ");
  lv_obj_align(pin_code_label, LV_ALIGN_TOP_MID, 0, 10);

  // Create buttons
  const char *btn_labels[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
  for (int i = 0; i < 10; ++i)
  {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_width, btn_height);
    int col = i % cols;
    int row = i / cols;
    int x = start_x + col * (btn_width + btn_spacing);
    int y = start_y + row * (btn_height + btn_spacing);
    lv_obj_set_pos(btn, x, y);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, btn_labels[i]);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
  }

  // Optionally, print debug information about layout
  Serial.print("Keypad positioned at X: ");
  Serial.print(start_x);
  Serial.print(", Y: ");
  Serial.println(start_y);
}

void accessSettings()
{
  lv_obj_t *screen = lv_scr_act();
  createPinEntryScreen();
}
/*PIN END*/

void checkNetworkAndDeviceStatus()
{
  if (strcmp(fundingSourceBuffer, "Blink") == 0)
  {
    if (!wifiStatus())
    {
      Serial.println("No network connection available. Checking again soon...");
      // Optionally, trigger a screen update or indicator that network is required but unavailable
      // displaySettingsScreen(); // Show or update no connection screen
      SerialPort1.write(185);
    }
    else 
    {
      SerialPort1.write(184);
    }
  }
  else if (strcmp(fundingSourceBuffer, "LNbits") == 0 && (currencyATM == "" || adminkey == "" || readkey == ""))
  {
    if (!wifiStatus())
    {
      Serial.println("Network not needed, but missing data for LNbits...");
      //SerialPort1.write(184);
    }
  }
}

void reconnectWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.begin(localssid, localpass);
  }
}

void create_settings_button(lv_obj_t *parent)
{
  // Initialize styles
  lv_style_init(&style_connected);
  lv_style_set_bg_color(&style_connected, lv_color_hex(0x00FF00)); // Green for connected

  lv_style_init(&style_disconnected);
  lv_style_set_bg_color(&style_disconnected, lv_color_hex(0xFF0000)); // Red for disconnected

  // Create a button
  btn_wifi = lv_btn_create(screen_main);
  lv_obj_set_size(btn_wifi, 20, 20);                   // Set the button size
  lv_obj_align(btn_wifi, LV_ALIGN_TOP_RIGHT, -10, 10); // Position the button

  // Apply initial style
  update_settings_button_style();

  // Attach an event handler to the button
  lv_obj_add_event_cb(btn_wifi, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

void update_settings_button_style()
{
  if (!wifiStatus())
  {
    lv_obj_add_style(btn_wifi, &style_disconnected, 0);
  }
  else
  {
    lv_obj_add_style(btn_wifi, &style_connected, 0);
  }
}

void settings_btn_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    accessSettings(); // Show or update no connection screen
    // Attempt to reconnect and update the button style accordingly
    // reconnectWiFi();
    // update_settings_button_style();
    // updateMainScreenLabel();
  }
}

bool isBlink()
{
  if (blinkapikey != "" || blinkwalletid != "" && (strcmp(fundingSourceBuffer, "Blink") == 0))
  {
    return true;
  }
  else
  {
    return false;
  }
}
bool isLNbits()
{
  if (currencyATM != "" || adminkey != "" || readkey != "" && (strcmp(fundingSourceBuffer, "LNbits") == 0))
  {
    return true;
  }
  else
  {
    return false;
  }
}
void createThankYouScreen()
{
  screen_thx = lv_obj_create(NULL); // Create a new screen

  String LVGL_THX = "THANK YOU!";
  lv_obj_t *thxTitle = lv_label_create(screen_thx);                // full screen as the parent
  lv_label_set_text(thxTitle, LVGL_THX.c_str());                   // set label text
  lv_obj_align(thxTitle, LV_ALIGN_CENTER, 0, 0);                   // Center but 20 from the top
  lv_obj_set_style_text_font(thxTitle, &lv_font_montserrat_48, 0); // Use the large font
  lv_obj_set_style_text_color(thxTitle, LV_COLOR_GREEN, 0);

  String LVGL_THX_DESC = "START OVER TO BURN MORE!";
  lv_obj_t *thxDesc = lv_label_create(screen_thx);                // full screen as the parent
  lv_label_set_text(thxDesc, LVGL_THX_DESC.c_str());              // set label text
  lv_obj_align(thxDesc, LV_ALIGN_CENTER, 0, 60);                  // Center but 20 from the top
  lv_obj_set_style_text_font(thxDesc, &lv_font_montserrat_16, 0); // Use the large font
  lv_obj_set_style_text_color(thxDesc, LV_COLOR_GREEN, 0);

  lv_scr_load(screen_thx);
}

/**
 * @brief Updates the burn text label with the selected currency.
 *
 * This function updates the burn text label with the selected currency and performs additional operations such as checking the price, balance, and updating the main screen label.
 *
 * @note The label must be created before calling this function.
 */
void updateBurnText()
{
  Serial.print("Free heap (updateBurnText Start): ");
  Serial.println(ESP.getFreeHeap());
  if (burnTextLabel) // Ensure the label has been created
  {
    String combinedText = "BURN YOUR " + currencySelected + " FOR SATS";
    lv_label_set_text(burnTextLabel, combinedText.c_str());

    checkNetworkAndDeviceStatus();
    checkPrice();
    checkBalance();
    updateMainScreenLabel();
  }
  Serial.print("Free heap (updateBurnText end): ");
  Serial.println(ESP.getFreeHeap());
}

const int INHIBIT_START = 131;
const int UNINHIBIT_START = 151;

void setCurrency(const String &newCurrency)
{
  Serial.println("Currency set to " + newCurrency);
  currencySelected = newCurrency;
  updateBurnText(); // Update the label text when the currency changes

  // Clear all channels before setting the new ones
  for (int i = 0; i < 16; i++)
  {
    SerialPort1.write(INHIBIT_START + i); // Inhibit all initially
    delay(200);
  }

  // Determine which channels to uninhibit based on the selected currency
  int startChannel = 0;
  int currencySize = 0;

  if (currencySelected == currencyone)
  {
    startChannel = 0;
    currencySize = originalSizeOne;
    lv_obj_add_state(btn1, LV_STATE_CHECKED);
    if (currencytwo != "")
    {
      lv_obj_clear_state(btn2, LV_STATE_CHECKED);
    }
    if (currencythree != "")
    {
      lv_obj_clear_state(btn3, LV_STATE_CHECKED);
    }
  }
  else if (currencySelected == currencytwo)
  {
    startChannel = originalSizeOne;
    currencySize = originalSizeTwo;
    lv_obj_clear_state(btn1, LV_STATE_CHECKED);
    lv_obj_add_state(btn2, LV_STATE_CHECKED);
    if (currencythree != "")
    {
      lv_obj_clear_state(btn3, LV_STATE_CHECKED);
    }
  }
  else if (currencySelected == currencythree)
  {
    startChannel = originalSizeOne + originalSizeTwo;
    currencySize = originalSizeThree;
    lv_obj_clear_state(btn1, LV_STATE_CHECKED);
    lv_obj_clear_state(btn2, LV_STATE_CHECKED);
    lv_obj_add_state(btn3, LV_STATE_CHECKED);
  }

  // Uninhibit channels for the selected currency
  for (int i = 0; i < currencySize; i++)
  {
    int channelCode = UNINHIBIT_START + startChannel + i;
    Serial.print("Sending value allow " + currencySelected + ": ");
    Serial.println(channelCode);
    SerialPort1.write(channelCode);
    delay(200);
  }
}

/**
 * @brief Checks the price of Bitcoin in the selected currency.
 *
 * This function sends a request to the CoinGecko API to get the current price of Bitcoin in the selected currency.
 * If the request is successful, the function parses the JSON response and retrieves the value of Bitcoin in the selected currency.
 *
 * @note This function requires an active Wi-Fi connection.
 */
void checkPrice()
{
  // Check Wi-Fi status
  // if (WiFi.status() == WL_CONNECTED)
  //{
  // Price API
  http.begin(primaryConversionAPI + currencySelected); // Specify request destination
  // http.begin("https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=" + currencySelected);              // Specify request destination
  // http.addHeader("Authorization", "Apikey 6f51e60ff3bbd092aac649ada41fb685523e30ab782c2d05ff11ee2acb351ad4"); // Specify API key header

  int httpCode = http.GET(); // Send the request

  if (httpCode == 200 || httpCode == 201)
  {                                    // Check the returning code
    String payload = http.getString(); // Get the request response payload
    // Serial.println(payload);
    //  Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String tempCurrency = currencySelected;
    tempCurrency.toLowerCase();

    // Get EUR value from parsed JSON
    fiatValue = doc["bitcoin"][tempCurrency.c_str()];
    Serial.print(F("HTTP (checkPrice): "));
    Serial.println(httpCode);
  }
  else
  {
    Serial.print(F("Error (checkPrice): "));
    Serial.println(httpCode);
  }
  Serial.print("Free heap (checkPrice): ");
  Serial.println(ESP.getFreeHeap());
  http.end(); // Close connection
  //}
}

/**
 * @brief Checks the balance of the selected currency using the specified funding source.
 *
 * This function retrieves the balance of the selected currency by making an HTTP request to the corresponding API endpoint.
 * The balance is then parsed from the response and converted to the corresponding fiat value.
 * The function also prints various debug information to the serial monitor.
 *
 * @note This function assumes that the necessary variables (currencySelected, fundingsource, baseURLATM, secretATM, chargeSelected, maxamountSelected, lnbitsURL, readkey, fiatValue, buffer, balanceSats, fiatBalance, http) have been properly initialized before calling this function.
 *
 * @note This function uses the HTTPClient library for making HTTP requests.
 *
 * @note This function should be called after selecting the currency and funding source.
 *
 * @note This function should be called within the loop function or any other appropriate place in the code.
 */
void checkBalance()
{
  if (currencySelected == currencyATM || currencySelected == currencyone)
  {
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM1;
      secretATM = secretATM1;
    }
    chargeSelected = charge1;
    maxamountSelected = maxamount;
  }
  else if (currencySelected == currencyATM2 || currencySelected == currencytwo)
  {
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM2;
      secretATM = secretATM2;
    }
    chargeSelected = charge2;
    maxamountSelected = maxamount2;
  }
  else if (currencySelected == currencyATM3 || currencySelected == currencythree)
  {
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM3;
      secretATM = secretATM3;
    }
    chargeSelected = charge3;
    maxamountSelected = maxamount3;
  }
  if (strcmp(fundingSourceBuffer, "LNbits") == 0)
  {
    http.begin(lnbitsURL + "/api/v1/wallet"); // Specify request destination
    http.addHeader("X-Api-Key", readkey);     // Specify API key header

    int httpCode = http.GET(); // Send the request

    if (httpCode == 200 || httpCode == 201)
    {                                      // Check the returning code
      String payloadon = http.getString(); // Get the request response payload
      Serial.println(payloadon);

      // Parse JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payloadon);

      // Get balance from parsed JSON
      balanceSats = doc["balance"];

      fiatBalance = ((double)balanceSats * fiatValue) / 100000000000.0;

      Serial.print(F("Balance: "));
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
      Serial.println(ESP.getFreeHeap());
    }
    else
    {
      Serial.print(F("Error (checkBalance): "));
      Serial.println(httpCode);
    }
  }
  else if (strcmp(fundingSourceBuffer, "Blink") == 0)
  {
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
    serializeJson(jsonDoc, requestBody); // Serialize the JSON object to a string

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
    if (httpCode == 200)
    {
      JsonObject me = respDoc["data"]["me"]["defaultAccount"]["wallets"][0]; // Assuming you want the first wallet
      String blinkwalletid = me["id"].as<String>();
      String walletCurrency = me["walletCurrency"].as<String>();
      balanceSats = me["balance"];

      fiatBalance = ((double)balanceSats / 100000000.0) * fiatValue;

      Serial.print("Wallet ID: ");
      Serial.println(blinkwalletid);
      Serial.print("Currency: ");
      Serial.println(walletCurrency);
      Serial.print("Balance: ");
      Serial.println(balanceSats);
      Serial.print("Fiat balance: ");
      Serial.println(fiatBalance);
    }
    else
    {
      Serial.println("Failed to fetch wallet information");
    }
  }

  http.end(); // Close connection
}

// Generic function to manage button states
static void update_button_states(lv_obj_t *active_btn)
{
  if (active_btn != btn1)
    lv_obj_clear_state(btn1, LV_STATE_CHECKED);
  if (active_btn != btn2 && currencytwo != "")
    lv_obj_clear_state(btn2, LV_STATE_CHECKED);
  if (active_btn != btn3 && currencythree != "")
    lv_obj_clear_state(btn3, LV_STATE_CHECKED);
  lv_obj_add_state(active_btn, LV_STATE_CHECKED);
}

static void btn1_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    setCurrency(currencyone);
    update_button_states(lv_event_get_target(e));
    Serial.println("Currency set to " + currencyone);
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM1;
      secretATM = secretATM1;
    }
    chargeSelected = charge1;
    maxamountSelected = maxamount;
  }
}

static void btn2_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    setCurrency(currencytwo);
    update_button_states(lv_event_get_target(e));
    Serial.println("Currency set to " + currencytwo);
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM2;
      secretATM = secretATM2;
    }
    chargeSelected = charge2;
    maxamountSelected = maxamount2;
  }
}

static void btn3_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED)
  {
    setCurrency(currencythree);
    update_button_states(lv_event_get_target(e));
    Serial.println("Currency set to " + currencythree);
    if (strcmp(fundingSourceBuffer, "LNbits") == 0)
    {
      baseURLATM = baseURLATM3;
      secretATM = secretATM3;
    }
    chargeSelected = charge3;
    maxamountSelected = maxamount3;
  }
}

// Create the main screen
/**
 * @brief Callback function for color animation.
 *
 * This function is called during a color animation and updates the text color of an object.
 * It takes a pointer to the object and an integer value as parameters.
 * The integer value represents the progress of the animation (0 to 255).
 * The function calculates the index in the colors array based on the progress value,
 * and sets the text color of the object to the corresponding color from the array.
 *
 * @param var Pointer to the object.
 * @param v Integer value representing the progress of the animation.
 */
void color_anim_cb(void *var, int32_t v)
{
  lv_obj_t *obj = (lv_obj_t *)var;
  int num_colors = sizeof(colors) / sizeof(colors[0]);

  int idx = (v * num_colors) / 256; // This will convert v (0 to 255) to an index in the colors array.
  lv_color_t color = colors[idx];

  lv_obj_set_style_text_color(obj, color, 0);
}

void createImages(lv_obj_t *parent)
{
  img_blink = lv_img_create(screen_main);
  lv_img_set_src(img_blink, &blink); // 'blink' must be a properly defined LVGL image variable
  lv_obj_align(img_blink, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);

  img_lnbits = lv_img_create(screen_main);
  lv_img_set_src(img_lnbits, &lnbits); // 'lnbits' must be a properly defined LVGL image variable
  lv_obj_align(img_lnbits, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);

  if (strcmp(fundingSourceBuffer, "LNbits") == 0)
  {
    lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
  }
  else if (strcmp(fundingSourceBuffer, "Blink") == 0)
  {
    lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
  }
}

/**
 * @brief Updates the main screen label with the current balance, fiat value, and charge value.
 *
 * This function checks if the balanceValueLabel, fiatValueLabel, and chargeValueLabel have been created
 * and initialized. If the WiFi status is offline, it sets the labels to display "OFFLINE" and changes the
 * text color to red. Otherwise, it formats and sets the text of the labels with the appropriate values
 * and changes the text color accordingly. The balanceValueLabel text color is set to white by default,
 * but if the fiat balance is less than the maximum amount selected, it changes the text color to red.
 *
 * @note The labels must be created and initialized before calling this function.
 */
void updateMainScreenLabel()
{
  Serial.print("Free heap (updateMainScreenLabel Start): ");
  Serial.println(ESP.getFreeHeap());
  if (balanceValueLabel)
  { // Ensure the label has been created
    if (!wifiStatus())
    {
      lv_label_set_text(balanceValueLabel, "OFFLINE");
    }
    else
    {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%.2f %s", fiatBalance, currencySelected);
      lv_label_set_text(balanceValueLabel, buffer);
      lv_obj_set_style_text_color(balanceValueLabel, LV_COLOR_WHITE, 0);
      if (fiatBalance < maxamountSelected)
      {
        lv_obj_set_style_text_color(balanceValueLabel, LV_COLOR_RED, 0);
      }
    }
  }
  if (fiatValueLabel)
  { // Check if it has been initialized
    if (!wifiStatus())
    {
      lv_label_set_text(fiatValueLabel, "OFFLINE");
      lv_obj_set_style_text_color(fiatValueLabel, LV_COLOR_RED, 0);
    }
    else
    {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%ld %s", (long)fiatValue, currencySelected);
      lv_label_set_text(fiatValueLabel, buffer);
      lv_obj_set_style_text_color(fiatValueLabel, LV_COLOR_GREEN, 0);
    }
  }
  if (chargeValueLabel)
  { // Check if it has been initialized
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%ld %%", chargeSelected);
    lv_label_set_text(chargeValueLabel, buffer);
  }

  Serial.print("Free heap (updateMainScreenLabel End): ");
  Serial.println(ESP.getFreeHeap());
}

/**
 * @brief Creates the main screen of the application.
 *
 * This function initializes and configures various UI elements on the main screen, such as labels, images, and buttons.
 * It sets the text and alignment of each element based on the provided parameters or default values.
 * The function also starts an animation for a specific label.
 * Finally, it loads the main screen and displays it on the screen.
 */
void createMainScreen()
{
  SerialPort1.write(184);
  digitalWrite(INHIBITMECH, HIGH);
  // if ((!wifiStatus()) && (strcmp(fundingSourceBuffer, "Blink") == 0))
  //   {
  //     Serial.println("No network connection available. Checking again soon...");
  //     SerialPort1.write(185);
  //     digitalWrite(INHIBITMECH, LOW);
  //     // Optionally, trigger a screen update or indicator that network is required but unavailable
  //     // displaySettingsScreen(); // Show or update no connection screen
  //   }
  Serial.println("createMainScreen: Start machine");
  Serial.println("createMainScreen: Start");
  Serial.print("Free heap (createMainScreen Start): ");
  Serial.println(ESP.getFreeHeap());

  screen_main = lv_obj_create(NULL); // Create a new screen
  Serial.println("createMainScreen: Screen created");

  String LVGL_Atm_desc = "BITCOIN LIGHTNING ATM ";
  if (atmdesc != "")
  {
    LVGL_Atm_desc = atmdesc;
  };
  lv_obj_t *label = lv_label_create(screen_main);  // full screen as the parent
  lv_label_set_text(label, LVGL_Atm_desc.c_str()); // set label text
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);    // Center but 20 from the top
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);

  String LVGL_Zero_Title = "";
  if (atmsubtitle != "")
  {
    LVGL_Zero_Title = atmsubtitle;
  };
  lv_obj_t *zeroline = lv_label_create(screen_main);    // full screen as the parent
  lv_label_set_text(zeroline, LVGL_Zero_Title.c_str()); // set label text
  lv_obj_align(zeroline, LV_ALIGN_TOP_MID, 0, 45);      // Center but 20 from the top
  if (atmsubtitle == "AMITY")
  {
    lv_label_set_text(zeroline, ""); // set label text
  }
  if (atmsubtitle == "DVADSATJEDEN")
  {
    lv_obj_set_style_text_font(zeroline, &lv_font_the_bold_48, 0); // Assuming lv_font_montserrat_22 is a bold font.
  }
  else
  {
    lv_obj_set_style_text_font(zeroline, &lv_font_montserrat_48, 0);
  }

  String LVGL_Fiat_Hell = "FIAT HELL";
  if (atmtitle != "")
  {
    LVGL_Fiat_Hell = atmtitle;
  };
  lv_obj_t *fiathell = lv_label_create(screen_main);                    // full screen as the parent
  lv_label_set_text(fiathell, LVGL_Fiat_Hell.c_str());                  // set label text
  lv_obj_align(fiathell, LV_ALIGN_TOP_MID, 0, 95);                      // Center but 20 from the top
  lv_obj_set_style_text_font(fiathell, &lv_font_montserrat_bold_60, 0); // Assuming lv_font_montserrat_22 is a bold font.

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, fiathell);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_time(&a, 500); // duration of one color change cycle
  lv_anim_set_exec_cb(&a, color_anim_cb);
  lv_anim_start(&a);
  // lv_task_handler();

  /* Create a label with big text */
  burnTextLabel = lv_label_create(screen_main); // Assign it to global variable
  String combinedText = "BURN YOUR " + currencySelected + " FOR SATS";
  lv_label_set_text(burnTextLabel, combinedText.c_str());
  lv_obj_set_style_text_font(burnTextLabel, &lv_font_montserrat_24, 0); // Use the large font
  lv_obj_align(burnTextLabel, LV_ALIGN_TOP_MID, 0, 163);                // Center but 20 from the top
  Serial.println("createMainScreen: burnTextLabel created");

  if (atmsubtitle == "DVADSATJEDEN")
  {
    lv_obj_t *img1 = lv_img_create(screen_main);   // Create an image object
    lv_img_set_src(img1, &btcSmallImg);            // Set the image source to your converted image (my_image)
    lv_obj_align(img1, LV_ALIGN_TOP_MID, 180, 70); // Align the image to the center of the screen
    Serial.println("createMainScreen: btc logo added");
  }

  if (atmsubtitle == "AMITY")
  {
    lv_obj_t *img1 = lv_img_create(screen_main);   // Create an image object
    lv_img_set_src(img1, &amityImg);            // Set the image source to your converted image (my_image)
    lv_obj_align(img1, LV_ALIGN_TOP_MID, 0, 15); // Align the image to the center of the screen
    Serial.println("createMainScreen: amity logo added");
  }

  lv_obj_t *labelBalance = lv_label_create(screen_main);     // full screen as the parent
  lv_label_set_text(labelBalance, "BALANCE");                // set label text
  lv_obj_align(labelBalance, LV_ALIGN_BOTTOM_LEFT, 30, -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelBalance, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelBalance created");

  lv_obj_t *labelPrice = lv_label_create(screen_main);   // full screen as the parent
  lv_label_set_text(labelPrice, "PRICE");                // set label text
  lv_obj_align(labelPrice, LV_ALIGN_BOTTOM_MID, 0, -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelPrice, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelPrice created");

  lv_obj_t *labelCharge = lv_label_create(screen_main);       // full screen as the parent
  lv_label_set_text(labelCharge, "FEE");                      // set label text
  lv_obj_align(labelCharge, LV_ALIGN_BOTTOM_RIGHT, -30, -40); // Center but 20 from the top
  lv_obj_set_style_text_font(labelCharge, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: labelCharge created");

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", fiatBalance, currencySelected.c_str()); // Limiting to 2 decimal places and append the currency
  balanceValueLabel = lv_label_create(screen_main);                                   // full screen as the parent
  lv_label_set_text(balanceValueLabel, buffer);                                       // set label text now that balanceValueLabel is created
  lv_obj_align(balanceValueLabel, LV_ALIGN_BOTTOM_LEFT, 30, -20);                     // Center but 20 from the top
  lv_obj_set_style_text_font(balanceValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: balanceValueLabel created");

  snprintf(buffer, sizeof(buffer), "%ld %s", (long)fiatValue, currencySelected.c_str()); // Display as whole number and append the currency
  fiatValueLabel = lv_label_create(screen_main);                                         // Create it on your main screen
  lv_label_set_text(fiatValueLabel, buffer);                                             // Initial text
  lv_obj_align(fiatValueLabel, LV_ALIGN_BOTTOM_MID, 0, -20);                             // Position it as you like
  lv_obj_set_style_text_font(fiatValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: fiatValueLabel created");

  snprintf(buffer, sizeof(buffer), "%d %%", chargeSelected);       // Convert the int to a char array
  chargeValueLabel = lv_label_create(screen_main);                 // Create it on your main screen
  lv_label_set_text(chargeValueLabel, buffer);                     // Set the label text using the buffer
  lv_obj_align(chargeValueLabel, LV_ALIGN_BOTTOM_RIGHT, -30, -20); // Position it as you like
  lv_obj_set_style_text_font(chargeValueLabel, &lv_font_montserrat_16, 0);
  Serial.println("createMainScreen: chargeValueLabel created");

  // if ((currencyATM2 != "" || currencyATM3 != "") || (currencytwo != "" || currencythree != ""))
  // {
  lv_button_currency();
  Serial.println("createMainScreen: lv_button_currency created");
  // }
  // else
  // {
  //   currencySelected = currencyone;
  //   //setCurrency(currencySelected);
  //   //updateBurnText();
  //   //Serial.println("createMainScreen: updateBurnText created");
  // }

  createImages(screen_main);
  create_settings_button(screen_main); // Add the WiFi button to the main screen

  // Switch fundingsource
  // if (strcmp(enableswitch, "Yes") == 0 && (isBlink && isLNbits))
  // {
  //   createSwitch();
  //   Serial.println("createMainScreen: createSwitch created");
  // }
  lv_scr_load(screen_main);
  Serial.println("createMainScreen: Screen loaded");
  Serial.print("Free heap (createMainScreen End): ");
  Serial.println(ESP.getFreeHeap());
}

/*void escrow_and_balance_task(lv_timer_t * t) {
  Serial.println("Inside escrow_and_balance_task");
  // Your balance checking logic here
  //checkBalance();
  SerialPort1.write(170); // Enable escrow mode

  if (SerialPort1.available())
  {
    Serial.println("Data available on SerialPort1");
    int channel = SerialPort1.read();
    Serial.print("Read data: ");
    Serial.println(channel);
    if (channel >= 1 && channel <= 16) {
      double noteValue = billAmountIntOne[channel]; // Replace 'yourMap' with the actual map or array you have of channel-value mappings
      Serial.print("Note value");
      Serial.println(noteValue);



      delay(1000); // Give some time for NV10 to act, replace with the actual needed delay
      checkBalance(); // Check balance immediately
      Serial.print("Fiat balance");
      Serial.println(fiatBalance);

      if (fiatBalance >= noteValue) {
        SerialPort1.write(172); // Accept the bill
      } else {
        SerialPort1.write(173); // Reject the bill
      }

      SerialPort1.write(171); // Disable escrow mode
    }
  } else {
    Serial.println("Serial port 1 unavailable");
  }
}*/

/**
 * @brief Creates the insert money screen.
 *
 * This function creates a new screen and adds labels for displaying the money inserted, total amount, prompt, and maximum amount.
 * It also sets the necessary styles for the labels.
 *
 * @note This function assumes that the main screen has already been deleted and the global variable `isInsertingMoney` has been set to `true`.
 *
 * @note This function prints the free heap size to the serial monitor.
 */
void createInsertMoneyScreen()
{
  deleteMainScreen(); // Properly manage deletion of the previous screen

  isInsertingMoney = true;

  Serial.println("Inside createInsertMoneyScreen()");
  Serial.print("Free heap (createInsertMoneyScreen): ");
  Serial.println(ESP.getFreeHeap());

  // Create a new screen
  lv_obj_t *screen_insert_money = lv_obj_create(NULL);
  if (!screen_insert_money)
  {
    Serial.println("Failed to create a new screen!");
    return;
  }

  // Create label for displaying the last inserted amount
  labelLastInserted = lv_label_create(screen_insert_money);
  if (labelLastInserted)
  {
    lv_label_set_text(labelLastInserted, ""); // Initialize with empty text
    lv_obj_align(labelLastInserted, LV_ALIGN_TOP_LEFT, 30, 50);
    lv_obj_set_style_text_font(labelLastInserted, &lv_font_montserrat_24, 0);
  }
  else
  {
    Serial.println("Failed to create labelLastInserted!");
  }

  // Create label for displaying the total amount
  labelTotalAmount = lv_label_create(screen_insert_money);
  if (labelTotalAmount)
  {
    lv_label_set_text(labelTotalAmount, ""); // Initialize with empty text
    lv_obj_align(labelTotalAmount, LV_ALIGN_TOP_LEFT, 30, 100);
    lv_obj_set_style_text_font(labelTotalAmount, &lv_font_montserrat_48, 0);
  }
  else
  {
    Serial.println("Failed to create labelTotalAmount!");
  }

  // Create prompt label
  lv_obj_t *labelPrompt = lv_label_create(screen_insert_money);
  if (labelPrompt)
  {
    lv_label_set_text(labelPrompt, "TAP SCREEN WHEN FINISHED");
    lv_obj_align(labelPrompt, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_text_font(labelPrompt, &lv_font_montserrat_16, 0);
  }
  else
  {
    Serial.println("Failed to create labelPrompt!");
  }

  // Create label for displaying the maximum amount
  labelMaxAmount = lv_label_create(screen_insert_money);
  if (labelMaxAmount)
  {
    lv_label_set_text(labelMaxAmount, ""); // Initialize with empty text
    lv_obj_align(labelMaxAmount, LV_ALIGN_TOP_LEFT, 30, 220);
    lv_obj_set_style_text_font(labelMaxAmount, &lv_font_montserrat_16, 0);
  }
  else
  {
    Serial.println("Failed to create labelMaxAmount!");
  }

  // Load the new screen
  lv_scr_load(screen_insert_money);
}

void switch_event_handler(lv_event_t *e)
{
  Serial.println("Handling switch event...");
  lv_obj_t *obj = lv_event_get_target(e);
  if (!obj || obj == NULL)
  {
    Serial.println("Error: Target object is null in event handler!");
    return;
  }

  // Also check images before using them
  if (!img_lnbits || !img_blink)
  {
    Serial.println("Error: Image objects are not initialized!");
    return;
  }

  if (lv_obj_has_state(obj, LV_STATE_CHECKED))
  {
    strcpy(fundingSourceBuffer, "LNbits");
    lv_label_set_text(switch_label, "LNbits");
    lv_obj_clear_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    strcpy(fundingSourceBuffer, "Blink");
    lv_label_set_text(switch_label, "Blink");
    lv_obj_clear_flag(img_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(img_lnbits, LV_OBJ_FLAG_HIDDEN);
  }

  // Since the fundingsource should already point to fundingSourceBuffer, no need to update it here
  checkNetworkAndDeviceStatus();
  checkBalance();
  updateBurnText();
  updateMainScreenLabel();

  Serial.print(F("Switch: Funding source: "));
  Serial.println(fundingSourceBuffer);
}

void createSwitch(lv_obj_t *parent)
{
  // Create a switch and add it to the screen
  switch_fund = lv_switch_create(screen_main);
  lv_obj_set_pos(switch_fund, 10, 30); // Set the position of the switch

  // Create a label for the switch
  switch_label = lv_label_create(screen_main);
  lv_label_set_text(switch_label, fundingSourceBuffer);                      // Use fundingSourceBuffer to set initial text
  lv_obj_align_to(switch_label, switch_fund, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Align label to the right of the switch

  // Assign an event handler to the switch
  lv_obj_add_event_cb(switch_fund, switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

  // Set the initial state from fundingSourceBuffer
  if (strcmp(fundingSourceBuffer, "LNbits") == 0)
  {
    lv_obj_add_state(switch_fund, LV_STATE_CHECKED); // Turn switch on if fundingsource is "lnbits"
    lv_label_set_text(switch_label, "LNbits");
  }
  else
  {
    lv_obj_clear_state(switch_fund, LV_STATE_CHECKED); // Ensure switch is off if fundingsource is "blink"
    lv_label_set_text(switch_label, "Blink");
  }
}

void set_initial_checked_button()
{
  // Example condition to decide which button should be checked
  if (currencySelected == currencyone)
  {
    lv_obj_add_state(btn1, LV_STATE_CHECKED);
  }
  if (currencySelected == currencytwo)
  {
    lv_obj_add_state(btn2, LV_STATE_CHECKED);
  }
  if (currencySelected == currencythree)
  {
    lv_obj_add_state(btn3, LV_STATE_CHECKED);
  }
  // updateBurnText();
}

/**
 * @brief Function to create and initialize currency buttons.
 *
 * This function creates and initializes currency buttons for EUR, USD, and CZK.
 * It sets the position, size, and text of each button based on the currency values.
 * It also sets the button style for the checked state and sets the initial currency.
 *
 * @note This function assumes that the variables currencyATM3, currencythree, currencyone, currencytwo, and currencySelected are defined and accessible.
 */
void lv_button_currency()
{
  lv_obj_t *labelbtn;

  // Initialize styles
  static lv_style_t style_btn_default, style_btn_checked;
  lv_style_init(&style_btn_default);
  lv_style_init(&style_btn_checked);

  // Default style properties
  lv_style_set_bg_color(&style_btn_default, lv_color_make(0, 0, 0)); // Black background
  lv_style_set_bg_opa(&style_btn_default, LV_OPA_COVER);

  // Checked style properties
  lv_style_set_bg_color(&style_btn_checked, lv_color_make(255, 165, 0)); // Orange color
  lv_style_set_bg_opa(&style_btn_checked, LV_OPA_COVER);

  // Create buttons and apply styles
  btn1 = lv_btn_create(screen_main);
  lv_obj_add_style(btn1, &style_btn_default, 0);                // Apply default style
  lv_obj_add_style(btn1, &style_btn_checked, LV_STATE_CHECKED); // Apply checked style
  lv_obj_add_event_cb(btn1, btn1_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_set_pos(btn1, currencyATM3 == "" || currencythree == "" ? 110 : 30, 200);
  lv_obj_set_size(btn1, 120, 50);
  labelbtn = lv_label_create(btn1);
  lv_label_set_text(labelbtn, currencyone.c_str());
  lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
  lv_obj_center(labelbtn);

  // Repeat for other buttons with respective modifications for btn2 and optionally btn3
  if (currencyATM2 != "" || currencytwo != "")
  {
    btn2 = lv_btn_create(screen_main);
    lv_obj_add_style(btn2, &style_btn_default, 0);
    lv_obj_add_style(btn2, &style_btn_checked, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn2, btn2_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn2, currencyATM3 == "" || currencythree == "" ? 260 : 180, 200);
    lv_obj_set_size(btn2, 120, 50);
    labelbtn = lv_label_create(btn2);
    lv_label_set_text(labelbtn, currencytwo.c_str());
    lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
    lv_obj_center(labelbtn);
  }

  if (currencyATM3 != "" || currencythree != "")
  {
    btn3 = lv_btn_create(screen_main);
    lv_obj_add_style(btn3, &style_btn_default, 0);
    lv_obj_add_style(btn3, &style_btn_checked, LV_STATE_CHECKED);
    lv_obj_add_event_cb(btn3, btn3_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_set_pos(btn3, 330, 200);
    lv_obj_set_size(btn3, 120, 50);
    labelbtn = lv_label_create(btn3);
    lv_label_set_text(labelbtn, currencythree.c_str());
    lv_obj_set_style_text_font(labelbtn, &lv_font_montserrat_24, 0);
    lv_obj_center(labelbtn);
  }

  // Set initial currency
  setCurrency(currencyone);
  // Serial.print(F("Set initial Currency: "));
  // Serial.println(currencySelected);

  // Set initial checked state after all buttons are created
  // Switch fundingsource
  set_initial_checked_button();
}

/*** Display callback to flush the buffer to screen ***/
/**
 * @brief Flushes the display with the provided color data in the specified area.
 *
 * This function is responsible for updating the display with the provided color data
 * in the specified area. It uses the startWrite(), setAddrWindow(), pushPixels(), and
 * endWrite() functions of the lcd object to perform the display update.
 *
 * @param disp Pointer to the display driver structure.
 * @param area Pointer to the area structure specifying the region to update.
 * @param color_p Pointer to the color data array.
 */
void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.pushPixels((uint16_t *)&color_p->full, w * h, true);
  lcd.endWrite();

  lv_disp_flush_ready(disp);
}

/*** Touchpad callback to read the touchpad ***/
void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  bool touched = lcd.getTouch(&touchX, &touchY);

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;

    // Serial.printf("Touch (x,y): (%03d,%03d)\n",touchX,touchY );
  }
}

/**
 * Retrieves a Bolt invoice from a specified URL and stores it in the 'boltInvoice' variable.
 * The URL is assumed to be provided in the 'callback' variable.
 * If the request is successful, the invoice is extracted from the JSON response and printed to the Serial monitor.
 * If the invoice is not found in the JSON response, an appropriate message is printed.
 * If the HTTP GET request fails, the function will retry after a delay of 3 seconds.
 */
void getBoltInvoice()
{
  // Assuming 'callback' is the URL where the invoice can be fetched
  http.begin(callback);                               // Initialize the connection to the URL
  http.addHeader("Content-Type", "application/json"); // Set header for JSON

  int httpCode = http.GET(); // Perform the GET request

  if (httpCode == 200)
  {                                             // Check if the request was successful
    String responseCallback = http.getString(); // Get the response as a string

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, responseCallback); // Parse the JSON response into the document

    boltInvoice = doc["invoice"].as<String>(); // Extract the invoice from the JSON document

    if (!boltInvoice.isEmpty())
    {
      Serial.print("Bolt Invoice: ");
      Serial.println(boltInvoice);
    }
    else
    {
      Serial.println("Invoice not found in the JSON response.");
    }
  }
  else
  {
    Serial.print("HTTP GET failed, error: ");
    Serial.println(httpCode); // Print the HTTP error code
    http.end();
    delay(5000);
    Serial.print("Waiting for Bolt11: ");
    Serial.println(ESP.getFreeHeap());
    getBoltInvoice();
  }

  http.end(); // Close the connection
}

/**
 * Sends a POST request to the GraphQL API endpoint with the provided Bolt invoice.
 * The request includes the necessary headers and payload to process the payment.
 *
 * @param boltInvoice The Bolt invoice to be sent as part of the request payload.
 */
void getBlinkLnURL(const String &boltInvoice)
{
  http.begin(graphqlEndpoint);                        // Initialize with the API endpoint
  http.addHeader("Content-Type", "application/json"); // Set content type
  http.addHeader("X-API-KEY", blinkapikey);           // Add the API key in the Authorization header

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
  doc["variables"]["input"]["paymentRequest"] = boltInvoice;
  doc["variables"]["input"]["memo"] = "LightningATM payout";

  String requestBody;
  serializeJson(doc, requestBody); // Serialize JSON document to a string

  // Make the POST request
  int httpCode = http.POST(requestBody);     // Send the request
  String responsePayload = http.getString(); // Get the response payload

  Serial.print("Modified LNURL: ");
  Serial.println(modifiedLnURLgen);
  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);
  Serial.print("Response Payload: ");
  Serial.println(responsePayload);

  http.end(); // Close connection
}

void createLNURLWithdraw()
{
  float temp = ((total / 100.0) / fiatValue * 1e8);

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);

  if (chargeSelected > 0)
  {
    tempCharge = ((total / 100.0) / fiatValue * 1e8) * chargeSelected / 100;
    result = round(temp) - tempCharge;
  }
  else
  {
    result = round(temp);
  }

  Serial.print("Charge TEMP: ");
  Serial.println(tempCharge);
  Serial.print("Result (rounded satoshis): ");
  Serial.println(result);

  // Convert long to String for the POST request
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
  if (httpCode != 200 && httpCode != 201)
  {
    // Primary service failed, try secondary service
    Serial.println("Primary service failed with code: " + String(httpCode));
    Serial.println("Attempting to connect to secondary service...");

    http.end(); // End connection to primary service
    http.begin(secondaryApiEndpoint);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(requestBody);
  }

  if (httpCode == 200 || httpCode == 201)
  {
    String payload = http.getString();
    Serial.print("Blink payload: ");
    Serial.println(payload);
    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    // Get balance from parsed JSON
    lnURLgen = doc["lnurl"].as<String>();
    modifiedLnURLgen = lnURLgen.substring(10);
    callback = doc["callback"].as<String>();
  }
  else
  {
    Serial.println("Failed to generate LNURL: " + String(httpCode));
  }

  http.end();
}

void getBlinkLNURL()
{
  Serial.print("Total (cents): ");
  Serial.println(total);
  Serial.print("EUR Value (price of 1 Bitcoin in euros): ");
  Serial.println(fiatValue);
  Serial.print("Charge: ");
  Serial.println(chargeSelected);

  float temp = ((total / 100.0) / fiatValue * 1e8);

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);

  if (chargeSelected > 0)
  {
    tempCharge = ((total / 100.0) / fiatValue * 1e8) * chargeSelected / 100;
    result = round(temp) - tempCharge;
  }
  else
  {
    result = round(temp);
  }

  Serial.print("Charge TEMP: ");
  Serial.println(tempCharge);
  Serial.print("Result (rounded satoshis): ");
  Serial.println(result);

  // Convert long to String for the POST request
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
  if (httpCode == 201)
  {
    String response = http.getString();
    Serial.println("Response from Blink: " + response);
  }
  else
  {
    Serial.println("Failed to execute operation via Blink: " + String(httpCode));
  }

  http.end();
}

/**
 * @brief Retrieves the LNURL from the server based on the provided parameters.
 *
 * This function calculates the LNURL based on the total amount, EUR value, and charge selected.
 * It then sends a POST request to the server to retrieve the LNURL.
 * The LNURL is parsed from the response and stored in the lnURLgen variable.
 *
 * @note This function assumes that the necessary variables (total, fiatValue, chargeSelected, lnbitsURL, adminkey) have been properly initialized.
 */
void getLNURL()
{
  Serial.print("Total (cents): ");
  Serial.println(total);
  Serial.print("EUR Value (price of 1 Bitcoin in euros): ");
  Serial.println(fiatValue);
  Serial.print("Charge: ");
  Serial.println(chargeSelected);

  float temp = ((total / 100.0) / fiatValue * 1e8);

  Serial.print("Temp (satoshis): ");
  Serial.println(temp);

  if (chargeSelected > 0)
  {
    tempCharge = ((total / 100.0) / fiatValue * 1e8) * chargeSelected / 100;
    result = round(temp) - tempCharge;
  }
  else
  {
    result = round(temp);
  }

  Serial.print("Charge TEMP: ");
  Serial.println(tempCharge);
  Serial.print("Result (rounded satoshis): ");
  Serial.println(result);

  // Convert long to String for the POST request
  String resultStr = String(result);

  http.begin(lnbitsURL + "/withdraw/api/v1/links");   // Specify request destination
  http.addHeader("Content-Type", "application/json"); // Specify content-type header
  http.addHeader("X-Api-Key", adminkey);              // Specify API key header

  String httpRequestData = "{\"title\": \"Fiat Hell ";
  httpRequestData += "\", \"min_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"max_withdrawable\": ";
  httpRequestData += resultStr;
  httpRequestData += ", \"uses\": 1, \"wait_time\": 1, \"is_unique\": 1, \"webhook_url\": \"\"}";
  int httpCode = http.POST(httpRequestData);

  // int httpCode = http.POST("{\"title\": \"Fiat Hell\", \"min_withdrawable\": \" + result + \", \"max_withdrawable\": \" + result + \" , \"uses\": \"1\", \"wait_time\": \"1\", \"is_unique\": \"true\", \"webhook_url\": \"\"}");   // Send the request
  String payloadon = http.getString(); // Get the response payload

  // Serial.println(httpCode);   // Print HTTP return code
  Serial.print("Temp: ");
  Serial.println(temp); // Print request response payload
  Serial.print("Result: ");
  Serial.println(result);
  Serial.print("ResultSTR: ");
  Serial.println(resultStr);
  Serial.print("Payload: ");
  Serial.println(payloadon); // Print request response payload

  // Parse JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payloadon);

  // Get balance from parsed JSON
  lnURLgen = doc["lnurl"].as<String>();

  Serial.print("LNURL: ");
  Serial.println(lnURLgen);
  modifiedLnURLgen = lnURLgen;

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
void makeLNURL()
{
  int randomPin = random(1000, 9999);
  byte nonce[8];
  for (int i = 0; i < 8; i++)
  {
    nonce[i] = random(256);
  }

  byte payload[51]; // 51 bytes is max one can get with xor-encryption

  size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t *)secretATM.c_str(), secretATM.length(), nonce, sizeof(nonce), randomPin, float(total));
  String preparedURL = baseURLATM + "?atm=1&p=";
  preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);

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
  qrData = charLnurl;
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
 * @return The number of bytes written to the output buffer, or 0 if there was not enough space.
 */
int xor_encrypt(uint8_t *output, size_t outlen, uint8_t *key, size_t keylen, uint8_t *nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
{
  // check we have space for all the data:
  // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
  if (outlen < 2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8)
  {
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
  uint8_t *payload = output + cur;                                 // pointer to the start of the payload
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
  for (int i = 0; i < payload_len; i++)
  {
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

void showMessageLVGL(String message)
{
  // Create an LVGL label to display the message
  lv_obj_t *label = lv_label_create(screen_qr);
  lv_label_set_text(label, message.c_str());
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void showQRCodeLVGL(const char *data)
{
  static lv_obj_t *screen_qr = nullptr;

  if (screen_qr != nullptr)
  {
    lv_obj_del(screen_qr); // Delete the previous screen
    screen_qr = nullptr;
    Serial.println("Previous screen deleted");
  }

  screen_qr = lv_obj_create(NULL); // Create a new screen
  if (screen_qr == nullptr)
  {
    Serial.println("Failed to create screen object.");
    return;
  }
  Serial.println("showQRCodeLVGL: Screen created");

  lv_color_t bg_color = lv_color_white();
  lv_color_t fg_color = lv_color_black();

  // Create the QR code with the expected arguments
  lv_obj_t *qr = lv_qrcode_create(screen_qr, 200, fg_color, bg_color);
  if (qr == nullptr)
  {
    Serial.println("Failed to create QR code object.");
    return;
  }

  // Update QR code with the given data
  if (lv_qrcode_update(qr, data, strlen(data)) != LV_RES_OK)
  {
    Serial.println("Failed to update QR code.");
    return;
  }
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, 25);

  // Add a border with bg_color
  lv_obj_set_style_border_color(qr, bg_color, 0);
  lv_obj_set_style_border_width(qr, 5, 0);

  // Create an LVGL label to display the LNURL
  lv_obj_t *labelLNURL = lv_label_create(screen_qr);
  if (labelLNURL == nullptr)
  {
    Serial.println("Failed to create label object.");
    return;
  }
  lv_label_set_long_mode(labelLNURL, LV_LABEL_LONG_WRAP); // Break the long lines
  lv_label_set_text(labelLNURL, modifiedLnURLgen.c_str());
  lv_obj_set_style_text_font(labelLNURL, &lv_font_montserrat_16, 0); // Use the large font
  lv_obj_set_style_text_color(labelLNURL, lv_color_hex(0xCCCCCC), 0);
  lv_obj_align(labelLNURL, LV_ALIGN_TOP_LEFT, 5, 5);
  // Get the display width
  uint32_t display_width = lv_disp_get_hor_res(NULL);
  // Set the label width to the display width
  lv_obj_set_size(labelLNURL, display_width - 5, LV_SIZE_CONTENT);

  // Create an LVGL label to display the message Warning
  lv_obj_t *labelWarning = lv_label_create(screen_qr);
  if (labelWarning == nullptr)
  {
    Serial.println("Failed to create label object labelWarning.");
    return;
  }  
  lv_label_set_text(labelWarning, "IN CASE OF PROBLEMS, MAKE A PHOTO AND CONTACT SUPPORT");
  lv_obj_set_style_text_font(labelWarning, &lv_font_montserrat_14, 0); // Use the large font
  lv_obj_set_style_text_color(labelWarning, lv_color_hex(0xCCCCCC), 0);
  lv_obj_align(labelWarning, LV_ALIGN_BOTTOM_MID, 0, -5);

  // Create an LVGL label to display the message
  lv_obj_t *label = lv_label_create(screen_qr);
  if (label == nullptr)
  {
    Serial.println("Failed to create label object.");
    return;
  }

  if (strcmp(fundingSourceBuffer, "LNbits") == 0)
  {
    lv_label_set_text(label, "TAP ON \nSCREEN \nWHEN \nFINISHED");
  }
  else
  {
    lv_label_set_text(label, "SCAN \nAND \nWAIT \nFOR \nCONFIR-\nMATION");
  }
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0); // Use the large font
  lv_obj_set_style_text_color(label, lv_color_hex(0xFF9900), 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 5, 0);

  lv_scr_load(screen_qr);

  Serial.print("Free heap (showQRCodeLVGL): ");
  Serial.println(ESP.getFreeHeap());

  printHeapStatus(); // Print heap status for debugging
}

void deleteMainScreen()
{
  if (screen_main != NULL)
  { // Check if screen_qr actually points to an object
    lv_obj_del(screen_main);
    screen_main = NULL; // Set the pointer to NULL to avoid "dangling pointers"
    Serial.println(F("Delete: screen_main"));
  }
}

void deleteInsertMoneyScreen()
{
  if (screen_insert_money != NULL)
  { // Check if screen_qr actually points to an object
    lv_obj_del(screen_insert_money);
    screen_insert_money = NULL; // Set the pointer to NULL to avoid "dangling pointers"
    Serial.println(F("Delete: screen_insert_money"));
  }
}

void deleteQRCodeScreen()
{
  if (screen_qr != NULL)
  { // Check if screen_qr actually points to an object
    lv_obj_del(screen_qr);
    screen_qr = NULL; // Set the pointer to NULL to avoid "dangling pointers"
    Serial.println(F("Delete: screen_qr"));
  }
}

void printHeapStatus()
{
  Serial.print("Total heap: ");
  Serial.println(ESP.getHeapSize());
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Largest free block: ");
  Serial.println(ESP.getMaxAllocHeap());
  Serial.print("Heap fragmentation: ");
  Serial.println((float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize() * 100.0);
}

// void checkNetworkAndDeviceStatus()
// {
//   bool wifiStatus = checkWiFiConnection(); // Implement this function to check WiFi status

//   if (!wifiStatus && (strcmp(fundingSourceBuffer, "Blink") == 0))
//   {
//     Serial.println("No Internet Connection. Disabling acceptor and waiting for connection...");

//     // Turn off machines
//     SerialPort1.write(185);         // Assuming this is the correct code to disable the acceptor
//     digitalWrite(INHIBITMECH, LOW); // Assuming this pin controls the acceptor's ability to operate

//     // Optionally, load a different screen/message
//     //displaySettingsScreen(); // Implement this to update the UI accordingly
//   }
// }

void displaySettingsScreen()
{
  lv_obj_t *screen_settings = lv_obj_create(NULL); // Get the current active screen or create a new one
  lv_scr_load(screen_settings);                    // Load the new screen as active

  createBackButton(screen_settings);  // Add back button to the settings screen
  createResetButton(screen_settings); // Add reset button to the settings screen

  // Create a label to inform the user
  lv_obj_t *label = lv_label_create(screen_settings);
  lv_label_set_text(label, "Settings");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10); // Position the label

  // Button to open configuration portal
  lv_obj_t *btn_config = lv_btn_create(screen_settings);
  lv_obj_set_size(btn_config, 120, 40);
  lv_obj_align(btn_config, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_t *btn_label_config = lv_label_create(btn_config);
  lv_label_set_text(btn_label_config, "Config Portal");
  lv_obj_center(btn_label_config);
  lv_obj_add_event_cb(btn_config, btn_config_portal_event_handler, LV_EVENT_CLICKED, NULL);

  // Create a label for switch
  lv_obj_t *label_switch = lv_label_create(screen_settings);
  lv_label_set_text(label_switch, "Funding source: \n(LNbits or Blink)");
  lv_obj_align(label_switch, LV_ALIGN_TOP_LEFT, 30, 30); // Position the label
  // Create a switch and add it to the screen
  switch_fund = lv_switch_create(screen_settings);
  lv_obj_set_pos(switch_fund, 30, 80); // Set the position of the switch

  // Create a label for the switch
  switch_label = lv_label_create(screen_settings);
  lv_label_set_text(switch_label, fundingSourceBuffer);                      // Use fundingSourceBuffer to set initial text
  lv_obj_align_to(switch_label, switch_fund, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Align label to the right of the switch

  // Assign an event handler to the switch
  lv_obj_add_event_cb(switch_fund, switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

  // Set the initial state from fundingSourceBuffer
  if (strcmp(fundingSourceBuffer, "LNbits") == 0)
  {
    lv_obj_add_state(switch_fund, LV_STATE_CHECKED); // Turn switch on if fundingsource is "lnbits"
    lv_label_set_text(switch_label, "LNbits");
  }
  else
  {
    lv_obj_clear_state(switch_fund, LV_STATE_CHECKED); // Ensure switch is off if fundingsource is "blink"
    lv_label_set_text(switch_label, "Blink");
  }
  lv_scr_load(screen_settings);
}

void btn_config_portal_event_handler(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    Serial.println("Button Clicked - Starting Config Portal");
    createPortalScreen();
    startConfigPortal();
  }
}

void btn_back_event_handler(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    memset(pin_code, 0, sizeof(pin_code)); // Clear pin_code when going back
    Serial.println("Back to Main Screen");
    createMainScreen(); // Navigate back to the main screen
  }
}

void switchFundingSource()
{
  // Toggle between Blink and another funding source
  if (strcmp(fundingSourceBuffer, "Blink") == 0)
  {
    strcpy(fundingSourceBuffer, "LNbits"); // Assume "Alternative" is another valid funding source
  }
  else
  {
    strcpy(fundingSourceBuffer, "Blink");
  }
  Serial.print("Funding source switched to: ");
  Serial.println(fundingSourceBuffer);
}

void startConfigPortal()
{
  Serial.println("Entered Config Portal");

  // Assume config and portal are previously defined and configured appropriately
  config.immediateStart = true;
  portal.join({elementsAux, saveAux, firstAux, savefirstAux, secondAux, savesecondAux, thirdAux, savethirdAux});
  portal.config(config);
  portal.begin();
  while (true)
  {
    portal.handleClient();
  }
  // timer = 2000;
}

/* Reset button */
// Function to create a reset button
void createResetButton(lv_obj_t *parent)
{
  if (parent == NULL)
    return;                              // Safety check
  lv_obj_t *btn = lv_btn_create(parent); // Create button on the provided parent object
  lv_obj_set_size(btn, 100, 40);         // Set button size
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -10, 10);

  lv_obj_t *label = lv_label_create(btn); // Create label on the button
  lv_label_set_text(label, "Reset");      // Set label text
  lv_obj_center(label);                   // Center the label within the button

  // Attach event handler to the button
  lv_obj_add_event_cb(btn, reset_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

// Event handler for the reset button
static void reset_btn_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    Serial.println("Reset button clicked, restarting...");
    ESP.restart(); // Command to soft reset the device (specific to ESP32/ESP8266)
  }
}

/* Back button */
// Function to create a reset button
void createBackButton(lv_obj_t *parent)
{
  if (parent == NULL)
    return;                                   // Safety check
  lv_obj_t *btn_back = lv_btn_create(parent); // Create button on the provided parent object
  lv_obj_set_size(btn_back, 80, 40);
  lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_t *btn_label_back = lv_label_create(btn_back);
  lv_label_set_text(btn_label_back, "Back");
  lv_obj_center(btn_label_back);
  lv_obj_add_event_cb(btn_back, btn_back_event_handler, LV_EVENT_CLICKED, NULL);
}

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
 * @brief The main loop function that runs repeatedly in the program.
 *
 * This function is responsible for handling the GUI, checking the balance, updating the main screen label,
 * detecting the insertion of money, processing the total, and waiting for user input to go back to the main screen.
 * It also includes a delay of 5 milliseconds at the end of each iteration.
 */
void loop()
{
  lv_timer_handler(); // Let the GUI do its work
  // portal.handleClient();
  if (initialCheck)
  {
    previousMillis = millis() - interval; // So that it gets executed immediately after setup
    initialCheck = false;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    checkNetworkAndDeviceStatus();
    checkPrice();
    checkBalance();          // Check the balance every 5 minutes
    updateMainScreenLabel(); // Update the label on the main screen with the new balance
    lv_task_handler();
  }

  // Check if user is inserting money
  int x = nonBlockingRead();

  if (x != -1) // Data available
  {
    for (int i = 0; i < billAmountIntOne.size(); i++) // Using .size() method on std::vector
    {
      if ((i + 1) == x)
      {
        // A valid bill is detected
        bills = bills + billAmountIntOne[i];
        total = (coins + bills);

        if (!isInsertingMoney)
        {
          createInsertMoneyScreen();
          lv_task_handler();
          isInsertingMoney = true;
        }

        String lastBillString = "Last bill: " + String(billAmountIntOne[i]) + " " + currencySelected;
        String totalString = "Total: " + String(total) + " " + currencySelected;
        String maxString = "MAX: " + String(maxamountSelected) + " " + currencySelected + " from " + fundingSourceBuffer;

        lv_label_set_text(labelLastInserted, lastBillString.c_str());
        lv_label_set_text(labelTotalAmount, totalString.c_str());
        lv_label_set_text(labelMaxAmount, maxString.c_str());

        break; // Exit the for loop as we found a match
      }
    }
  }

  // Check button release or total
  BTNA.read();
  // Serial.print("Waiting for tap 1");
  if ((BTNA.wasReleased() && total != 0) || total >= maxamountSelected)
  {
    // Process the total and reset variables for the next transaction.
    total = (coins + bills) * 100;

    Serial.print(F("Total: "));
    Serial.println(total);

    if (!wifiStatus())
    {
      makeLNURL();
      printHeapStatus();
      Serial.println("makeLNURL() - LNbits offline");
      deleteInsertMoneyScreen();
      Serial.println("deleteInsertMoneyScreen() - LNbits offline");
      showQRCodeLVGL(qrData.c_str());
      Serial.print("showQRCodeLVGL() - LNbits offline");
      Serial.println(qrData);
      // Turn off machines
      SerialPort1.write(185);
      digitalWrite(INHIBITMECH, LOW);
      Serial.print("Free heap (makeLNURL): ");
      Serial.println(ESP.getFreeHeap());
      lv_task_handler();
      Serial.println("lv_task_handler() - LNbits offline");
      delay(5);
    }
    else
    {
      if (strcmp(fundingSourceBuffer, "Blink") == 0)
      {
        createLNURLWithdraw();
        Serial.println("createLNURLWithdraw() - Blink online");
        deleteInsertMoneyScreen();
        Serial.println("deleteInsertMoneyScreen() - Blink online");
        // Display the QR code for online
        showQRCodeLVGL(lnURLgen.c_str());
        Serial.println("showQRCodeLVGL() - Blink online");
        lv_task_handler();
        Serial.println("lv_task_handler() - Blink online");
        // Turn off machines
        SerialPort1.write(185);
        digitalWrite(INHIBITMECH, LOW);
        // delay(30000); // Wait for 30 seconds for the user to scan the QR code
        //  bool waitForTap = true;
        //  while (waitForTap)
        //  {
        //    BTNA.read();
        //    // Serial.print("Waiting for tap 2");
        //    if (BTNA.wasReleased())
        //    {
        //      waitForTap = false;
        //      // Reset for the next transaction
        //      // coins = 0;
        //      // bills = 0;
        //      // total = 0;
        //      // isInsertingMoney = false;
        //      // Load your main screen or perform any other desired action
        //      getBoltInvoice();
        //      getBlinkLnURL(boltInvoice);

        //   }
        // }
        getBoltInvoice();
        getBlinkLnURL(boltInvoice);
        deleteQRCodeScreen();
        createThankYouScreen();
        lv_task_handler();
        delay(3000);
        ESP.restart();
      }
      if (strcmp(fundingSourceBuffer, "LNbits") == 0)
      {
        getLNURL();
        Serial.println("getLNURL()");
        deleteInsertMoneyScreen();
        Serial.println("deleteInsertMoneyScreen() - LNbits online");
        // Display the QR code for online
        showQRCodeLVGL(lnURLgen.c_str());
        Serial.println("showQRCodeLVGL() - LNbits online");
        lv_task_handler();
        Serial.println("lv_task_handler() - LNbits online");
        // Turn off machines
        SerialPort1.write(185);
        digitalWrite(INHIBITMECH, LOW);
      }
      Serial.print("Free heap (showQRCodeLVGL): ");
      Serial.println(ESP.getFreeHeap());
    }

    // Now wait for a tap to go back to the main screen or any other action
    bool waitForTap = true;
    while (waitForTap)
    {
      BTNA.read();
      // Serial.print("Waiting for tap 2");
      if (BTNA.wasReleased())
      {
        waitForTap = false;
        // Reset for the next transaction
        coins = 0;
        bills = 0;
        total = 0;
        isInsertingMoney = false;
        // Load your main screen or perform any other desired action
        deleteQRCodeScreen();
        // createMainScreen();
        ESP.restart();
      }
    }
  }

  lv_task_handler(); // Call LVGL task handler
  delay(5);          // Small delay to avoid watchdog reset
}