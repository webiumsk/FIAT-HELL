#include "services/FundingService.h"
#include "services/HttpsClient.h"
#include "payout/Bolt11.h"
#include "PriceBalanceTask.h"

#include <ArduinoJson.h>

// S3 replaces this with a copy done under the price-task mutex. WT32 does not
// link that task, so the fallback writes straight through.
__attribute__((weak)) void priceBalancePublishWallet(DeviceState &ds,
                                                     SessionState &ss,
                                                     const char *walletId,
                                                     long balanceSats) {
  if (walletId != nullptr) {
    strlcpy(ds.blinkwalletid, walletId, sizeof(ds.blinkwalletid));
  }
  ss.balanceSats = balanceSats;
}

namespace FundingService {

static char g_payoutFailure[160];
static char g_pollNote[120];
static bool g_payoutFatal = false;

const char *payoutFailureReason() { return g_payoutFailure; }

void setPayoutFailure(const char *reason) {
  strlcpy(g_payoutFailure, reason ? reason : "", sizeof(g_payoutFailure));
}

void clearPayoutFailure() {
  g_payoutFailure[0] = '\0';
  g_pollNote[0] = '\0';
  g_payoutFatal = false;
}

bool payoutIsFatal() { return g_payoutFatal; }

const char *lastPollNote() { return g_pollNote; }

static void notePoll(const char *text) {
  strlcpy(g_pollNote, text ? text : "", sizeof(g_pollNote));
}

static const char *const blinkGraphqlEndpoint = "https://api.blink.sv/graphql";
static const char *const flashGraphqlEndpoint =
    "https://api.flashapp.me/graphql";
static const char *const primaryProxyEndpoint = "https://api.lnbc.sk/v1/lnurl";
static const char *const secondaryProxyEndpoint =
    "https://api.lnurlproxy.me/v1/lnurl";

bool isGaloy(const char *fundingSource) {
  return fundingSource && (strcmp(fundingSource, "Blink") == 0 ||
                           strcmp(fundingSource, "Flash") == 0);
}

const char *galoyEndpoint(const char *fundingSource) {
  return (fundingSource && strcmp(fundingSource, "Flash") == 0)
             ? flashGraphqlEndpoint
             : blinkGraphqlEndpoint;
}

const char *galoyWalletCurrency(const char *fundingSource) {
  return (fundingSource && strcmp(fundingSource, "Flash") == 0) ? "USD" : "BTC";
}

const char *galoyWalletCurrency(const char *fundingSource,
                                const char *blinkWallet) {
  if (fundingSource && strcmp(fundingSource, "Flash") == 0) {
    return "USD"; // Flash always pays from the custodial cash wallet
  }
  // Blink: operator-chosen wallet. Stablesats is the USD wallet; anything
  // else (including an empty setting) is the Bitcoin wallet.
  if (blinkWallet && strcmp(blinkWallet, "Stablesats") == 0) {
    return "USD";
  }
  return "BTC";
}

// Flash is migrating Cash wallets from IBEX-USD to USDT ("cash wallet
// cutover"). Without this capability header the API presents the legacy USD
// wallet id whose IBEX account is empty - payments from it fail with
// INSUFFICIENT_BALANCE while the real funds sit in the USDT wallet.
static void addFlashCapabilityHeader(HTTPClient &http,
                                     const char *fundingSource) {
  if (fundingSource && strcmp(fundingSource, "Flash") == 0) {
    http.addHeader("x-flash-client-capabilities", "cash-wallet-usdt-v1");
  }
}

bool fetchGaloyAuthorization(const DeviceState &ds, char *scopesOut,
                             size_t scopesLen) {
  if (scopesOut != nullptr && scopesLen > 0) {
    scopesOut[0] = '\0';
  }
  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(galoyEndpoint(ds.fundingSourceBuffer))) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", String(ds.blinkapikey));
  addFlashCapabilityHeader(http, ds.fundingSourceBuffer);
  DynamicJsonDocument doc(512);
  doc["query"] = "query Authorization { authorization { scopes } }";
  String requestBody;
  serializeJson(doc, requestBody);
  const int code = http.POST(requestBody);
  String payload = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("authorization[Galoy]: HTTP %d\n", code);
    return false;
  }
  DynamicJsonDocument resp(1024);
  if (deserializeJson(resp, payload) != DeserializationError::Ok) {
    Serial.println("authorization[Galoy]: JSON parse failed");
    return false;
  }
  String scopes;
  for (JsonVariant scope :
       resp["data"]["authorization"]["scopes"].as<JsonArray>()) {
    if (scopes.length() > 0) {
      scopes += ",";
    }
    scopes += scope.as<const char *>();
  }
  if (scopesOut != nullptr && scopesLen > 0) {
    strlcpy(scopesOut, scopes.c_str(), scopesLen);
  }
  return true;
}

bool checkLNbitsWallet(const DeviceState &ds, long *balanceSatsOut) {
  if (ds.lnbitsURL[0] == '\0' || ds.readkey[0] == '\0') {
    return false;
  }
  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(String(ds.lnbitsURL) + "/api/v1/wallet")) {
    return false;
  }
  http.addHeader("X-Api-Key", ds.readkey);
  const int code = http.GET();
  String payload = http.getString();
  http.end();
  if (code != 200 && code != 201) {
    Serial.printf("balance[LNbits]: HTTP %d\n", code);
    return false;
  }
  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println("balance[LNbits]: JSON parse failed");
    return false;
  }
  if (balanceSatsOut != nullptr) {
    *balanceSatsOut = doc["balance"];
  }
  return true;
}

bool fetchGaloyBalance(DeviceState &ds, SessionState &ss,
                       const char *walletCurrency) {
  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(galoyEndpoint(ds.fundingSourceBuffer))) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", String(ds.blinkapikey));
  addFlashCapabilityHeader(http, ds.fundingSourceBuffer);

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

  // A read-only key answers the balance query but every payout fails. Log
  // the scopes once per fetch so the limitation is visible before cash goes
  // in (the boot self-check blocks on this via fetchGaloyAuthorization).
  {
    char scopes[64] = "";
    if (fetchGaloyAuthorization(ds, scopes, sizeof(scopes))) {
      Serial.printf("balance[Galoy]: key scopes: %s\n",
                    scopes[0] ? scopes : "(none)");
    }
  }

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

  // Pick the funding wallet. For "USD" (Flash cash mode) prefer the active
  // USDT wallet with legacy-USD fallback (cash wallet cutover); otherwise
  // match walletCurrency exactly. Wallets with a null balance are external
  // (non-custodial) - the server cannot spend them, skip them.
  JsonArray wallets = respDoc["data"]["me"]["defaultAccount"]["wallets"];
  const bool cashMode = strcmp(walletCurrency, "USD") == 0;
  JsonObject chosen;
  for (JsonObject wallet : wallets) {
    const char *cur = wallet["walletCurrency"] | "";
    if (wallet["balance"].isNull()) continue;
    if (!cashMode) {
      if (strcmp(cur, walletCurrency) == 0) {
        chosen = wallet;
        break;
      }
    } else {
      if (strcmp(cur, "USDT") == 0) {
        chosen = wallet;
        break;
      }
      if (strcmp(cur, "USD") == 0 && chosen.isNull()) {
        chosen = wallet; // keep looking - a USDT wallet still wins
      }
    }
  }

  if (!chosen.isNull()) {
    const long balanceSats = chosen["balance"]; // sats for BTC, cents for USD
    priceBalancePublishWallet(ds, ss, chosen["id"] | "", balanceSats);
    Serial.printf("balance[Galoy]: %lld (%s wallet)\n",
                  (long long)ss.balanceSats,
                  (const char *)(chosen["walletCurrency"] | ""));
    return true;
  }

  Serial.printf("balance[Galoy]: no spendable %s wallet in the account\n",
                walletCurrency);
  return false;
}

bool payInvoice(const DeviceState &ds, const char *invoice,
                const char *walletIdOverride) {
  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(galoyEndpoint(ds.fundingSourceBuffer))) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", ds.blinkapikey);
  addFlashCapabilityHeader(http, ds.fundingSourceBuffer);
  // Lightning routing can legitimately take a while.
  http.setTimeout(30000);

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

  invoice = bolt11PaymentRequest(invoice);
  if (invoice == nullptr || invoice[0] == '\0') {
    Serial.println("payInvoice: empty invoice");
    setPayoutFailure("Invoice was empty");
    http.end();
    return false;
  }

  const char *payWalletId =
      (walletIdOverride && walletIdOverride[0] != '\0') ? walletIdOverride
                                                        : ds.blinkwalletid;
  Serial.print("payInvoice walletId: ");
  Serial.println(payWalletId);

  // Key names and alignment sit on top of the query and the invoice.
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1) +
                          JSON_OBJECT_SIZE(3) + graphqlQuery.length() +
                          strlen(payWalletId) + strlen(invoice) + 512;
  DynamicJsonDocument doc(capacity);
  doc["query"] = graphqlQuery;
  doc["variables"]["input"]["walletId"] = payWalletId;
  doc["variables"]["input"]["paymentRequest"] = invoice;
  doc["variables"]["input"]["memo"] = "LightningATM payout";
  if (doc.overflowed()) {
    Serial.println("payInvoice: invoice did not fit the payment request");
    setPayoutFailure("Invoice did not fit the Blink request");
    http.end();
    return false;
  }

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
    char msg[80];
    snprintf(msg, sizeof(msg), "Blink HTTP %d", httpCode);
    setPayoutFailure(msg);
    return false;
  }

  DynamicJsonDocument respDoc(responsePayload.length() + 512);
  if (deserializeJson(respDoc, responsePayload)) {
    Serial.println("Payment response parse error");
    setPayoutFailure("Blink response was not JSON");
    return false;
  }

  // Top-level errors may be present-but-empty; only a non-empty array is a
  // failure (same pattern as fetchGaloyBalance).
  if (respDoc["errors"].is<JsonArray>() && respDoc["errors"].size() > 0) {
    const char *msg = respDoc["errors"][0]["message"] | "(no message)";
    Serial.printf("Payment failed: GraphQL error: %s\n", msg);
    char shown[140];
    snprintf(shown, sizeof(shown), "Blink: %.110s", msg);
    setPayoutFailure(shown);
    return false;
  }

  const char *status = respDoc["data"]["lnInvoicePaymentSend"]["status"] | "";
  // PENDING is a payment Blink has accepted and is still routing. It is
  // treated as paid because the ATM has no later status check.
  if (strcmp(status, "SUCCESS") == 0 || strcmp(status, "PENDING") == 0) {
    return true;
  }
  // ALREADY_PAID means this invoice was settled by an earlier payment, so
  // this sale sent the customer nothing. Showing the thank-you screen would
  // hide that from both of them.
  if (strcmp(status, "ALREADY_PAID") == 0) {
    Serial.println("Payment refused: invoice was already paid");
    setPayoutFailure("Invoice was already paid");
    return false;
  }

  Serial.print("Payment failed, status: ");
  Serial.println(status);
  const char *errMsg =
      respDoc["data"]["lnInvoicePaymentSend"]["errors"][0]["message"] | "";
  if (errMsg[0] != '\0') {
    Serial.print("Payment error message: ");
    Serial.println(errMsg);
  }
  char shown[140];
  if (strstr(errMsg, "not authorized") != nullptr) {
    strlcpy(shown, "Blink API key cannot send payments (needs Write)",
            sizeof(shown));
  } else if (errMsg[0] != '\0') {
    snprintf(shown, sizeof(shown), "Blink %.20s: %.90s",
             status[0] ? status : "error", errMsg);
  } else {
    snprintf(shown, sizeof(shown), "Blink status %.40s",
             status[0] ? status : "empty");
  }
  setPayoutFailure(shown);
  return false;
}

bool requestLnurlWithdraw(SessionState &ss, long amountSats) {
  // Clear previous withdraw state up front so a failed request can't leave a
  // stale QR/callback from an earlier transaction behind.
  clearPayoutFailure();
  ss.lnURLgen[0] = '\0';
  ss.modifiedLnURLgen[0] = '\0';
  ss.callback[0] = '\0';

  DynamicJsonDocument doc(1024);
  doc["amount"] = amountSats;
  doc["memo"] = "Fiat Hell ATM";

  String requestBody;
  serializeJson(doc, requestBody);
  Serial.print("LNURL-withdraw requestBody: ");
  Serial.println(requestBody);

  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(primaryProxyEndpoint)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(requestBody);
  if (httpCode != 200 && httpCode != 201) {
    Serial.println("Primary proxy failed with code: " + String(httpCode));
    Serial.println("Attempting to connect to secondary proxy...");
    http.end();
    if (!session.begin(secondaryProxyEndpoint)) {
      return false;
    }
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(requestBody);
  }

  bool ok = false;
  if (httpCode == 200 || httpCode == 201) {
    String responsePayload = http.getString();
    Serial.print("Proxy payload: ");
    Serial.println(responsePayload);

    DynamicJsonDocument respDoc(1024);
    DeserializationError parseErr = deserializeJson(respDoc, responsePayload);
    if (parseErr) {
      Serial.print("LNURL proxy response parse error: ");
      Serial.println(parseErr.c_str());
    } else {
      strlcpy(ss.lnURLgen, respDoc["lnurl"] | "", sizeof(ss.lnURLgen));
      if (strlen(ss.lnURLgen) > 10) {
        strlcpy(ss.modifiedLnURLgen, ss.lnURLgen + 10,
                sizeof(ss.modifiedLnURLgen));
      }
      strlcpy(ss.callback, respDoc["callback"] | "", sizeof(ss.callback));
      // Both are required downstream: the QR shows lnURLgen and the invoice
      // polling loop GETs callback — without it polling would just time out.
      ok = ss.lnURLgen[0] != '\0' && ss.callback[0] != '\0';
      if (!ok) {
        Serial.println("LNURL proxy response missing lnurl/callback");
      }
    }
  } else {
    Serial.println("Failed to generate LNURL: " + String(httpCode));
  }

  http.end();
  return ok;
}

bool pollBoltInvoice(SessionState &ss) {
  if (ss.callback[0] == '\0') {
    Serial.println("Error: callback URL is empty");
    return false;
  }

  HttpsSession session;
  HTTPClient &http = session.httpClient;
  if (!session.begin(ss.callback)) {
    notePoll("callback connection failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode != 200) {
    // Not ready yet, this is normal until the wallet submits an invoice.
    char note[40];
    snprintf(note, sizeof(note), "HTTP %d", httpCode);
    notePoll(note);
    Serial.printf("Blink poll HTTP %d\n", httpCode);
    http.end();
    return false;
  }

  String responseCallback = http.getString();
  http.end();

  DynamicJsonDocument doc(responseCallback.length() + 512);
  DeserializationError error = deserializeJson(doc, responseCallback);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    char note[80];
    snprintf(note, sizeof(note), "bad JSON %.40s", error.c_str());
    notePoll(note);
    return false;
  }

  const char *inv = doc["invoice"] | "";
  if (inv[0] == '\0') {
    inv = doc["pr"] | "";
  }
  if (inv[0] == '\0') {
    inv = doc["payment_request"] | "";
  }
  inv = bolt11PaymentRequest(inv);
  if (inv && inv[0] != '\0') {
    if (strlcpy(ss.boltInvoice, inv, sizeof(ss.boltInvoice)) >=
        sizeof(ss.boltInvoice)) {
      Serial.println("Bolt invoice is longer than the ATM can pay");
      ss.boltInvoice[0] = '\0';
      setPayoutFailure("Invoice is too long for the ATM");
      g_payoutFatal = true;
      return false;
    }
    Serial.print("Bolt Invoice received: ");
    Serial.println(ss.boltInvoice);
    return true;
  }

  Serial.print("Invoice not found in poll body: ");
  Serial.println(responseCallback.substring(0, 180));
  char note[120];
  snprintf(note, sizeof(note), "no invoice: %.90s", responseCallback.c_str());
  notePoll(note);
  return false;
}

} // namespace FundingService
