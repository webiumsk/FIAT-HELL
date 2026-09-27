#pragma once

#include "DeviceState.h"
#include <ArduinoJson.h>
#if FIAT_HAS_AUTOCONNECT
#include <AutoConnect.h>
#endif
#include <FS.h>
#include <WString.h>
#include <cstring>
#include <initializer_list>
#include <vector>

struct GuiConfig {
  char fundingSource[100]{0};
  char rateSource[100]{0};
  char animated[100]{0};
  bool valid{false};
};

struct FirstConfig {
  char blinkApiKey[128]{0};
  char blinkWalletId[128]{0};
  char baseUrl[256]{0};
  char secret[256]{0};
  char currencyATM[64]{0};
  char currencyLabel[64]{0};
  char adminKey[256]{0};
  char readKey[256]{0};
  std::vector<int> billMech;
  int maxAmount{0};
  float charge{0.0f};
  bool valid{false};
};

struct SecondConfig {
  char baseUrl[256]{0};
  char secret[256]{0};
  char currencyATM[64]{0};
  char currencyLabel[64]{0};
  std::vector<int> billMech;
  int maxAmount{0};
  float charge{0.0f};
  bool valid{false};
};

struct ThirdConfig {
  char baseUrl[256]{0};
  char secret[256]{0};
  char currencyATM[64]{0};
  char currencyLabel[64]{0};
  std::vector<int> billMech;
  int maxAmount{0};
  float charge{0.0f};
  bool valid{false};
};

class ConfigService {
public:
  bool loadGuiConfig(fs::FS &fs, const char *path, GuiConfig &out);
  bool saveGuiConfig(fs::FS &fs, const char *path, const GuiConfig &in);

#if FIAT_HAS_AUTOCONNECT
  // AutoConnect portal helpers - WT32 only. S3 serves its own /setup page.
  bool loadAuxConfig(fs::FS &fs, const char *path, AutoConnectAux &aux,
                     std::initializer_list<const char *> keys);

  bool saveAuxConfig(fs::FS &fs, const char *path, AutoConnectAux &aux,
                     std::initializer_list<const char *> keys, String &echoOut);
#endif

  bool loadFirst(fs::FS &fs, const char *path, FirstConfig &out);

  /**
   * Overwrite only the Blink/Flash API key (positional entry 0) in
   * /first.json, preserving everything else. Creates a minimal file with
   * the expected 9-entry layout when none exists yet. Used by the
   * on-device Flash key wizard.
   */
  bool updateFirstBlinkApiKey(fs::FS &fs, const char *path,
                              const char *apiKey);
  bool loadSecond(fs::FS &fs, const char *path, SecondConfig &out);
  bool loadThird(fs::FS &fs, const char *path, ThirdConfig &out);

  /**
   * LNURL-withdraw proxy list: {"endpoints":["https://...", ...]}.
   * Fills ds.proxyEndpoints in file order (invalid/duplicate entries are
   * skipped, at most DeviceState::kMaxProxyEndpoints). When the file is
   * missing or yields nothing usable, ds gets the built-in defaults so the
   * ATM always has a proxy. Returns (and sets ds.proxyFromFile) true when
   * the file was used.
   */
  bool loadProxyConfig(fs::FS &fs, const char *path, DeviceState &ds);

  /**
   * Save a newline/comma separated list of proxy URLs. An input with no
   * valid URL removes the file, so the built-in defaults apply again.
   */
  bool saveProxyConfig(fs::FS &fs, const char *path, const String &list);

  /** Built-in defaults used when /proxy.json is absent, joined by sep. */
  static String defaultProxyEndpoints(const char *sep);
};
