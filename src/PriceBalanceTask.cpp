/**
 * @file PriceBalanceTask.cpp
 * @brief Background task for fetching price and balance without blocking main
 * loop.
 */

#include "PriceBalanceTask.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char *coinyepConversionAPI =
    "https://coinyep.com/api/v1/?from=BTC&to=";
static const char *coingeckoAPI =
    "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=";
static const char *krakenTickerAPI =
    "https://api.kraken.com/0/public/Ticker";
static const char *exchangeapiConversionAPI =
    "https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@latest/v1/"
    "currencies/btc.json";
static const char *graphqlEndpoint = "https://api.blink.sv/graphql";

static DeviceState *g_deviceState = nullptr;
static SessionState *g_sessionState = nullptr;
static QueueHandle_t g_requestQueue = nullptr;
static SemaphoreHandle_t g_dataMutex = nullptr;
static volatile bool g_dataReadyForUi = false;
static HTTPClient g_taskHttp;

static bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

static void taskFetchPrice(HTTPClient &http, const char *currencySelected,
                          const char *rateSourceBuffer, float *outFiatValue) {
  if (!outFiatValue)
    return;
  String targetCurrency = String(currencySelected);
  targetCurrency.toUpperCase();

  if (strcmp(rateSourceBuffer, "CoinGecko") == 0) {
    String curr = String(currencySelected);
    curr.toLowerCase();
    http.begin(String(coingeckoAPI) + curr);
    int code = http.GET();
    if (code == 200 || code == 201) {
      String payload = http.getString();
      DynamicJsonDocument doc(512);
      if (deserializeJson(doc, payload) == DeserializationError::Ok &&
          doc["bitcoin"][curr.c_str()]) {
        *outFiatValue = doc["bitcoin"][curr.c_str()].as<float>();
      }
    }
    http.end();
    return;
  }
  if (strcmp(rateSourceBuffer, "Kraken") == 0) {
    String pair = "XBTEUR";
    String curr = String(currencySelected);
    curr.toUpperCase();
    if (curr == "EUR") pair = "XBTEUR";
    else if (curr == "USD") pair = "XBTUSD";
    else if (curr == "CZK") pair = "XBTCZK";
    else if (curr == "GBP") pair = "XBTGBP";
    else pair = "XBT" + curr;
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
          *outFiatValue = String(lastStr).toFloat();
          break;
        }
      }
    }
    http.end();
    return;
  }
  if (strcmp(rateSourceBuffer, "ExchangeApi") == 0) {
    http.begin(exchangeapiConversionAPI);
    int code = http.GET();
    if (code == 200 || code == 201) {
      String payload = http.getString();
      DynamicJsonDocument doc(16384);
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        String tempCurrency = String(currencySelected);
        tempCurrency.toLowerCase();
        *outFiatValue = doc["btc"][tempCurrency].as<float>();
      }
    }
    http.end();
  } else {
    http.begin(String(coinyepConversionAPI) + targetCurrency);
    int code = http.GET();
    if (code == 200 || code == 201) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        const char *priceStr = doc["price"] | "";
        *outFiatValue = String(priceStr).toFloat();
      }
    }
    http.end();
  }
}

static void taskFetchBalance(HTTPClient &http, DeviceState &ds,
                             SessionState &ss, float fiatValue) {
  const char *currencySelected = ss.currencySelected;
  const char *fundingSource = ds.fundingSourceBuffer;

  if (strcmp(currencySelected, ds.currencyATM) == 0 ||
      strcmp(currencySelected, ds.currencyOne) == 0) {
    if (strcmp(fundingSource, "LNbits") == 0) {
      strlcpy(ss.baseURLATM, ds.baseURLATM1, sizeof(ss.baseURLATM));
      strlcpy(ss.secretATM, ds.secretATM1, sizeof(ss.secretATM));
    }
    ss.chargeSelected = ds.charge1;
    ss.maxamountSelected = ds.maxamount;
  } else if (strcmp(currencySelected, ds.currencyATM2) == 0 ||
             strcmp(currencySelected, ds.currencyTwo) == 0) {
    if (strcmp(fundingSource, "LNbits") == 0) {
      strlcpy(ss.baseURLATM, ds.baseURLATM2, sizeof(ss.baseURLATM));
      strlcpy(ss.secretATM, ds.secretATM2, sizeof(ss.secretATM));
    }
    ss.chargeSelected = ds.charge2;
    ss.maxamountSelected = ds.maxamount2;
  } else if (strcmp(currencySelected, ds.currencyATM3) == 0 ||
             strcmp(currencySelected, ds.currencyThree) == 0) {
    if (strcmp(fundingSource, "LNbits") == 0) {
      strlcpy(ss.baseURLATM, ds.baseURLATM3, sizeof(ss.baseURLATM));
      strlcpy(ss.secretATM, ds.secretATM3, sizeof(ss.secretATM));
    }
    ss.chargeSelected = ds.charge3;
    ss.maxamountSelected = ds.maxamount3;
  }

  if (strcmp(fundingSource, "LNbits") == 0) {
    String url = String(ds.lnbitsURL) + "/api/v1/wallet";
    http.begin(url);
    http.addHeader("X-Api-Key", ds.readkey);
    int code = http.GET();
    if (code == 200 || code == 201) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        ss.balanceSats = doc["balance"];
        ss.fiatBalance =
            ((double)ss.balanceSats * fiatValue) / 100000000000.0;
      }
    }
    http.end();
  } else if (strcmp(fundingSource, "Blink") == 0) {
    http.begin(graphqlEndpoint);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", String(ds.blinkapikey));
    const char *query = R"(
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
    DynamicJsonDocument jsonDoc(1024);
    jsonDoc["query"] = query;
    String requestBody;
    serializeJson(jsonDoc, requestBody);
    int code = http.POST(requestBody);
    if (code == 200) {
      String payload = http.getString();
      DynamicJsonDocument respDoc(4096);
      if (deserializeJson(respDoc, payload) == DeserializationError::Ok) {
        JsonArray wallets =
            respDoc["data"]["me"]["defaultAccount"]["wallets"];
        // Blink returns BTC wallet(s); use first wallet (same as original
        // checkBalance) - balance is in sats, fiatValue converts to display
        // currency (EUR, etc.)
        if (wallets.size() > 0) {
          JsonObject w = wallets[0];
          ss.balanceSats = w["balance"];
          ss.fiatBalance =
              ((double)ss.balanceSats / 100000000.0) * fiatValue;
        }
      }
    }
    http.end();
  }
}

static void priceBalanceTaskFunc(void *param) {
  (void)param;
  PriceBalanceRequest req;
  for (;;) {
    if (xQueueReceive(g_requestQueue, &req, portMAX_DELAY) != pdTRUE)
      continue;
    if (!g_deviceState || !g_sessionState)
      continue;
    if (!wifiConnected())
      continue;

    float fiatValue = 0.0f;
    taskFetchPrice(g_taskHttp, g_sessionState->currencySelected,
                  g_deviceState->rateSourceBuffer, &fiatValue);

    // Fetch rates for all 3 currencies (for mixed-currency insert)
    float fv1 = 0.0f, fv2 = 0.0f, fv3 = 0.0f;
    if (g_deviceState->currencyOne[0] != '\0')
      taskFetchPrice(g_taskHttp, g_deviceState->currencyOne,
                    g_deviceState->rateSourceBuffer, &fv1);
    if (g_deviceState->currencyTwo[0] != '\0')
      taskFetchPrice(g_taskHttp, g_deviceState->currencyTwo,
                    g_deviceState->rateSourceBuffer, &fv2);
    if (g_deviceState->currencyThree[0] != '\0')
      taskFetchPrice(g_taskHttp, g_deviceState->currencyThree,
                    g_deviceState->rateSourceBuffer, &fv3);

    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      g_sessionState->fiatValue = fiatValue;
      g_sessionState->fiatValue1 = fv1;
      g_sessionState->fiatValue2 = fv2;
      g_sessionState->fiatValue3 = fv3;
      taskFetchBalance(g_taskHttp, *g_deviceState, *g_sessionState, fiatValue);
      g_dataReadyForUi = true;
      xSemaphoreGive(g_dataMutex);
    }
  }
}

void startPriceBalanceTask(DeviceState *ds, SessionState *ss) {
  g_deviceState = ds;
  g_sessionState = ss;
  g_requestQueue = xQueueCreate(4, sizeof(PriceBalanceRequest));
  g_dataMutex = xSemaphoreCreateMutex();
  if (!g_requestQueue || !g_dataMutex)
    return;
  xTaskCreate(priceBalanceTaskFunc, "priceBal", 8192, nullptr, 1, nullptr);
}

void triggerPriceBalanceFetch(PriceBalanceRequest req) {
  if (g_requestQueue)
    xQueueSend(g_requestQueue, &req, 0);
}

bool isPriceBalanceDataReady() { return g_dataReadyForUi; }

bool consumePriceBalanceDataReady() {
  if (!g_dataReadyForUi)
    return false;
  g_dataReadyForUi = false;
  return true;
}
