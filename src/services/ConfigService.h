#pragma once

#include <ArduinoJson.h>
#include <AutoConnect.h>
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

  bool loadAuxConfig(fs::FS &fs, const char *path, AutoConnectAux &aux,
                     std::initializer_list<const char *> keys);

  bool saveAuxConfig(fs::FS &fs, const char *path, AutoConnectAux &aux,
                     std::initializer_list<const char *> keys, String &echoOut);

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
};
