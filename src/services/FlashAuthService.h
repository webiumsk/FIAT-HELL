#pragma once

#include <Arduino.h>
#include <HTTPClient.h>

/**
 * One-shot Flash (flashapp.me) account operations used by the on-device
 * "get API key" wizard. The SMS login code must be requested through the
 * Flash mobile app (the API's code-request mutations are captcha-gated);
 * userLogin itself is not, so the device can finish the flow:
 * phone + code -> session token -> apiKeyCreate.
 */
namespace FlashAuthService {

/**
 * Exchange phone + SMS code for a session token.
 * Returns false and fills errOut with a human-readable reason on failure.
 */
bool userLogin(HTTPClient &http, const String &phone, const String &code,
               String &authTokenOut, String &errOut);

/**
 * Create an API key with all scopes the ATM needs (read_user is mandatory
 * for the balance query). The raw key is only ever returned once.
 */
bool apiKeyCreate(HTTPClient &http, const String &authToken,
                  String &apiKeyOut, String &errOut);

} // namespace FlashAuthService
