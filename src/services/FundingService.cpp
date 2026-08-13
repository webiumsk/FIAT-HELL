#include "services/FundingService.h"

#include <ArduinoJson.h>

namespace FundingService {

static const char *blinkGraphqlEndpoint = "https://api.blink.sv/graphql";
static const char *flashGraphqlEndpoint = "https://api.flashapp.me/graphql";
static const char *primaryProxyEndpoint = "https://api.lnbc.sk/v1/lnurl";
static const char *secondaryProxyEndpoint = "https://api.lnurlproxy.me/v1/lnurl";

bool isGaloy(const char *fundingSource) {
  return fundingSource && (strcmp(fundingSource, "Blink") == 0 ||
                           strcmp(fundingSource, "Flash") == 0);
}

const char *galoyEndpoint(const char *fundingSource) {
  return (fundingSource && strcmp(fundingSource, "Flash") == 0)
             ? flashGraphqlEndpoint
             : blinkGraphqlEndpoint;
}

bool fetchGaloyBalance(HTTPClient &http, DeviceState &ds, SessionState &ss,
                       const char *walletCurrency) {
  http.begin(galoyEndpoint(ds.fundingSourceBuffer));
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
  if (code != 200) {
    Serial.printf("balance[Galoy]: HTTP %d (%s) — keeping cached %lld sats\n",
                  code, HTTPClient::errorToString(code).c_str(),
                  (long long)ss.balanceSats);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument respDoc(4096);
  if (deserializeJson(respDoc, payload) != DeserializationError::Ok) {
    Serial.println("balance[Galoy]: JSON parse failed");
    return false;
  }
  // Galoy returns errors in the payload even with HTTP 200
  if (respDoc["errors"].is<JsonArray>() && respDoc["errors"].size() > 0) {
    const char *msg = respDoc["errors"][0]["message"] | "(no message)";
    Serial.printf("balance[Galoy]: GraphQL error: %s\n", msg);
    return false;
  }

  // Pick the wallet matching walletCurrency — the account may also hold a
  // fiat/stablesats wallet and the order of the array is not guaranteed.
  JsonArray wallets = respDoc["data"]["me"]["defaultAccount"]["wallets"];
  for (JsonObject wallet : wallets) {
    if (strcmp(wallet["walletCurrency"] | "", walletCurrency) == 0) {
      strlcpy(ds.blinkwalletid, wallet["id"] | "", sizeof(ds.blinkwalletid));
      ss.balanceSats = wallet["balance"];
      Serial.printf("balance[Galoy]: %lld sats (%s wallet)\n",
                    (long long)ss.balanceSats, walletCurrency);
      return true;
    }
  }

  Serial.printf("balance[Galoy]: no %s wallet in the account\n",
                walletCurrency);
  return false;
}

bool payInvoice(HTTPClient &http, const DeviceState &ds, const char *invoice) {
  http.begin(galoyEndpoint(ds.fundingSourceBuffer));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", ds.blinkapikey);

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

  DynamicJsonDocument doc(1024);
  doc["query"] = graphqlQuery;
  doc["variables"]["input"]["walletId"] = ds.blinkwalletid;
  doc["variables"]["input"]["paymentRequest"] = invoice;
  doc["variables"]["input"]["memo"] = "LightningATM payout";

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  String responsePayload = http.getString();
  http.end();

  Serial.print("payInvoice HTTP Status Code: ");
  Serial.println(httpCode);
  Serial.print("payInvoice Response Payload: ");
  Serial.println(responsePayload);

  if (httpCode != 200) {
    Serial.println("Payment request failed at HTTP level");
    return false;
  }

  DynamicJsonDocument respDoc(2048);
  if (deserializeJson(respDoc, responsePayload)) {
    Serial.println("Payment response parse error");
    return false;
  }

  if (!respDoc["errors"].isNull()) {
    Serial.println("Payment failed: GraphQL error");
    return false;
  }

  const char *status = respDoc["data"]["lnInvoicePaymentSend"]["status"] | "";
  if (strcmp(status, "SUCCESS") == 0 || strcmp(status, "PENDING") == 0 ||
      strcmp(status, "ALREADY_PAID") == 0) {
    return true;
  }

  Serial.print("Payment failed, status: ");
  Serial.println(status);
  const char *errMsg =
      respDoc["data"]["lnInvoicePaymentSend"]["errors"][0]["message"] | "";
  if (errMsg[0] != '\0') {
    Serial.print("Payment error message: ");
    Serial.println(errMsg);
  }
  return false;
}

bool requestLnurlWithdraw(HTTPClient &http, SessionState &ss,
                          long amountSats) {
  DynamicJsonDocument doc(1024);
  doc["amount"] = amountSats;
  doc["memo"] = "Fiat Hell ATM";

  String requestBody;
  serializeJson(doc, requestBody);
  Serial.print("LNURL-withdraw requestBody: ");
  Serial.println(requestBody);

  http.begin(primaryProxyEndpoint);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(requestBody);
  if (httpCode != 200 && httpCode != 201) {
    Serial.println("Primary proxy failed with code: " + String(httpCode));
    Serial.println("Attempting to connect to secondary proxy...");
    http.end();
    http.begin(secondaryProxyEndpoint);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(requestBody);
  }

  bool ok = false;
  if (httpCode == 200 || httpCode == 201) {
    String responsePayload = http.getString();
    Serial.print("Proxy payload: ");
    Serial.println(responsePayload);

    DynamicJsonDocument respDoc(1024);
    deserializeJson(respDoc, responsePayload);

    strlcpy(ss.lnURLgen, respDoc["lnurl"] | "", sizeof(ss.lnURLgen));
    if (strlen(ss.lnURLgen) > 10) {
      strlcpy(ss.modifiedLnURLgen, ss.lnURLgen + 10,
              sizeof(ss.modifiedLnURLgen));
    } else {
      ss.modifiedLnURLgen[0] = '\0';
    }
    strlcpy(ss.callback, respDoc["callback"] | "", sizeof(ss.callback));
    ok = ss.lnURLgen[0] != '\0';
  } else {
    Serial.println("Failed to generate LNURL: " + String(httpCode));
  }

  http.end();
  return ok;
}

bool pollBoltInvoice(HTTPClient &http, SessionState &ss) {
  if (ss.callback[0] == '\0') {
    Serial.println("Error: callback URL is empty");
    return false;
  }

  http.begin(ss.callback);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode != 200) {
    // Not ready yet, this is normal - invoice hasn't been submitted
    http.end();
    return false;
  }

  String responseCallback = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, responseCallback);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  const char *inv = doc["invoice"];
  if (inv && strlen(inv) > 0) {
    strlcpy(ss.boltInvoice, inv, sizeof(ss.boltInvoice));
    Serial.print("Bolt Invoice received: ");
    Serial.println(ss.boltInvoice);
    return true;
  }

  Serial.println("Invoice not found in the JSON response.");
  return false;
}

} // namespace FundingService
