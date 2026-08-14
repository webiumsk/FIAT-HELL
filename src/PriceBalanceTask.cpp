/**
 * @file PriceBalanceTask.cpp
 * @brief Background task for fetching price and balance without blocking main
 * loop.
 */

#include "PriceBalanceTask.h"
#include "services/FundingService.h"
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
      } else {
        Serial.printf("price[CoinGecko/%s]: JSON parse/missing field\n",
                      targetCurrency.c_str());
      }
    } else {
      Serial.printf("price[CoinGecko/%s]: HTTP %d (%s)\n",
                    targetCurrency.c_str(), code,
                    HTTPClient::errorToString(code).c_str());
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
      } else {
        Serial.printf("price[Kraken/%s]: JSON parse/error\n", pair.c_str());
      }
    } else {
      Serial.printf("price[Kraken/%s]: HTTP %d (%s)\n", pair.c_str(), code,
                    HTTPClient::errorToString(code).c_str());
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
      } else {
        Serial.printf("price[ExchangeApi/%s]: JSON parse failed\n",
                      targetCurrency.c_str());
      }
    } else {
      Serial.printf("price[ExchangeApi/%s]: HTTP %d (%s)\n",
                    targetCurrency.c_str(), code,
                    HTTPClient::errorToString(code).c_str());
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
      } else {
        Serial.printf("price[CoinYEP/%s]: JSON parse failed\n",
                      targetCurrency.c_str());
      }
    } else {
      Serial.printf("price[CoinYEP/%s]: HTTP %d (%s)\n",
                    targetCurrency.c_str(), code,
                    HTTPClient::errorToString(code).c_str());
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
        Serial.printf("balance[LNbits]: %lld sats\n", (long long)ss.balanceSats);
      } else {
        Serial.println("balance[LNbits]: JSON parse failed");
      }
    } else {
      Serial.printf("balance[LNbits]: HTTP %d (%s) — keeping cached %lld sats\n",
                    code, HTTPClient::errorToString(code).c_str(),
                    (long long)ss.balanceSats);
    }
    http.end();
  } else if (FundingService::isGaloy(fundingSource)) {
    // Blink and Flash share the Galoy API; the service also picks the BTC
    // wallet (accounts may hold a fiat wallet first) and caches its id.
    if (FundingService::fetchGaloyBalance(http, ds, ss, "BTC")) {
      ss.fiatBalance = ((double)ss.balanceSats / 100000000.0) * fiatValue;
    }
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
      // Only overwrite cached values when the fetch actually succeeded.
      // A failed fetch leaves the local variable at 0; without this guard
      // every transient network glitch nukes the last-known price.
      if (fiatValue > 0.0f) g_sessionState->fiatValue  = fiatValue;
      if (fv1 > 0.0f)       g_sessionState->fiatValue1 = fv1;
      if (fv2 > 0.0f)       g_sessionState->fiatValue2 = fv2;
      if (fv3 > 0.0f)       g_sessionState->fiatValue3 = fv3;
      // Use the freshest available price for balance conversion
      const float priceForBalance =
          (fiatValue > 0.0f) ? fiatValue : g_sessionState->fiatValue;
      taskFetchBalance(g_taskHttp, *g_deviceState, *g_sessionState,
                       priceForBalance);
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
  const BaseType_t created =
      xTaskCreate(priceBalanceTaskFunc, "priceBal", 8192, nullptr, 1, nullptr);
  if (created != pdPASS) {
    Serial.println("PriceBalanceTask: xTaskCreate failed");
    vQueueDelete(g_requestQueue);
    g_requestQueue = nullptr;
  }
}

void triggerPriceBalanceFetch(PriceBalanceRequest req) {
  if (g_requestQueue)
    xQueueSend(g_requestQueue, &req, 0);
}

PriceBalanceWalletIdResult priceBalanceCopyWalletId(char *dst, size_t dstSize,
                                                    uint32_t timeoutMs) {
  if (!g_deviceState || !g_dataMutex)
    return PB_WALLETID_TASK_NOT_RUNNING;
  if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE)
    return PB_WALLETID_TIMEOUT;
  strlcpy(dst, g_deviceState->blinkwalletid, dstSize);
  xSemaphoreGive(g_dataMutex);
  return PB_WALLETID_OK;
}

bool isPriceBalanceDataReady() { return g_dataReadyForUi; }

bool consumePriceBalanceDataReady() {
  if (!g_dataReadyForUi)
    return false;
  if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    g_dataReadyForUi = false;
    xSemaphoreGive(g_dataMutex);
    return true;
  }
  return false;
}
