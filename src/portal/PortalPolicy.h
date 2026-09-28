#pragma once

#include <string.h>

// Setup-portal password. "changeme" is the firmware default and is refused,
// as is anything shorter than 8 characters. WPA2 refuses a passphrase above
// 63 characters, so those are refused too.
inline bool portalPasswordAccepted(const char *password) {
  if (password == nullptr) {
    return false;
  }
  const size_t length = strlen(password);
  if (length < 8 || length > 63) {
    return false;
  }
  return strcmp(password, "changeme") != 0;
}

// Config, OTA and the Flash key wizard. The customer home page is not in
// this list. Matching is exact except for the /_ac, /ota and /flashkey
// prefixes AutoConnect and the wizard use.
inline bool portalPathIsSensitive(const char *uri) {
  if (uri == nullptr) {
    return false;
  }
  static const char *exact[] = {
      "/config",      "/save",      "/first",       "/savefirst",
      "/second",      "/savesecond", "/third",      "/savethird",
      "/gui",         "/savegui",   nullptr};
  for (int i = 0; exact[i] != nullptr; i++) {
    if (strcmp(uri, exact[i]) == 0) {
      return true;
    }
  }
  return strncmp(uri, "/_ac", 4) == 0 || strncmp(uri, "/ota", 4) == 0 ||
         strncmp(uri, "/flashkey", 9) == 0;
}
