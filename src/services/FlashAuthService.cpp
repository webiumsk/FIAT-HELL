#include "services/FlashAuthService.h"
#include "services/FundingService.h"

#include <ArduinoJson.h>

namespace FlashAuthService {

static const char *flashEndpoint() {
  return FundingService::galoyEndpoint("Flash");
}

// POST a GraphQL request; returns HTTP code and fills responseOut.
static int postGraphql(HTTPClient &http, const String &requestBody,
                       const String &bearerToken, String &responseOut) {
  http.begin(flashEndpoint());
  http.addHeader("Content-Type", "application/json");
  if (bearerToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + bearerToken);
  }
  http.setTimeout(15000);
  const int code = http.POST(requestBody);
  responseOut = http.getString();
  http.end();
  return code;
}

bool userLogin(HTTPClient &http, const String &phone, const String &code,
               String &authTokenOut, String &errOut) {
  DynamicJsonDocument doc(1024);
  doc["query"] =
      "mutation($input: UserLoginInput!) { userLogin(input: $input) { "
      "authToken errors { message } } }";
  doc["variables"]["input"]["phone"] = phone;
  doc["variables"]["input"]["code"] = code;
  String body;
  serializeJson(doc, body);

  String response;
  const int httpCode = postGraphql(http, body, "", response);
  Serial.printf("flashkey userLogin: HTTP %d\n", httpCode);
  if (httpCode != 200) {
    errOut = "HTTP " + String(httpCode) + " - skontroluj internet zariadenia";
    return false;
  }

  DynamicJsonDocument resp(2048);
  if (deserializeJson(resp, response)) {
    errOut = "Neplatna odpoved servera";
    return false;
  }
  if (resp["errors"].is<JsonArray>() && resp["errors"].size() > 0) {
    errOut = String((const char *)(resp["errors"][0]["message"] | "chyba"));
    return false;
  }
  JsonObject login = resp["data"]["userLogin"];
  if (login["errors"].is<JsonArray>() && login["errors"].size() > 0) {
    errOut = String((const char *)(login["errors"][0]["message"] | "chyba"));
    return false;
  }
  const char *token = login["authToken"] | "";
  if (token[0] == '\0') {
    errOut = "Server nevratil token - kod je asi nespravny alebo expirovany";
    return false;
  }
  authTokenOut = token;
  return true;
}

bool apiKeyCreate(HTTPClient &http, const String &authToken,
                  String &apiKeyOut, String &errOut) {
  DynamicJsonDocument doc(1024);
  doc["query"] =
      "mutation($input: ApiKeyCreateInput!) { apiKeyCreate(input: $input) { "
      "apiKey { apiKey } errors { message } } }";
  JsonObject input = doc["variables"].createNestedObject("input");
  input["name"] = "FIAT-HELL ATM";
  JsonArray scopes = input.createNestedArray("scopes");
  // read_user is mandatory: without it the `me` balance query fails.
  scopes.add("read_user");
  scopes.add("read_wallet");
  scopes.add("write_wallet");
  scopes.add("read_transactions");
  scopes.add("write_transactions");
  String body;
  serializeJson(doc, body);

  String response;
  const int httpCode = postGraphql(http, body, authToken, response);
  Serial.printf("flashkey apiKeyCreate: HTTP %d\n", httpCode);
  if (httpCode != 200) {
    errOut = "HTTP " + String(httpCode) + " pri vytvarani kluca";
    return false;
  }

  DynamicJsonDocument resp(2048);
  if (deserializeJson(resp, response)) {
    errOut = "Neplatna odpoved servera";
    return false;
  }
  if (resp["errors"].is<JsonArray>() && resp["errors"].size() > 0) {
    errOut = String((const char *)(resp["errors"][0]["message"] | "chyba"));
    return false;
  }
  JsonObject payload = resp["data"]["apiKeyCreate"];
  if (payload["errors"].is<JsonArray>() && payload["errors"].size() > 0) {
    errOut = String((const char *)(payload["errors"][0]["message"] | "chyba"));
    return false;
  }
  const char *key = payload["apiKey"]["apiKey"] | "";
  if (key[0] == '\0') {
    errOut = "Server nevratil kluc";
    return false;
  }
  apiKeyOut = key;
  return true;
}

} // namespace FlashAuthService
