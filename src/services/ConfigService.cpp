#include "services/ConfigService.h"

bool ConfigService::loadGuiConfig(fs::FS &fs, const char *path,
                                  GuiConfig &out) {
  File guiFile = fs.open(path, "r");
  if (!guiFile) {
    return false;
  }

  DynamicJsonDocument docGui(2400);
  DeserializationError error = deserializeJson(docGui, guiFile);
  guiFile.close();
  if (error) {
    return false;
  }

  auto fillFromArray = [](JsonObject obj, const char *key, char *dest,
                          size_t destSize) {
    JsonArray values = obj["value"];
    int checkedIndex = obj["checked"];
    if (checkedIndex > 0 && checkedIndex <= values.size()) {
      strlcpy(dest, values[checkedIndex - 1], destSize);
    }
  };

  const JsonObject docGui0 = docGui[0];
  if (!docGui0.isNull()) {
    fillFromArray(docGui0, "fundingsource", out.fundingSource,
                  sizeof(out.fundingSource));
  }

  const JsonObject docGui1 = docGui[1];
  if (!docGui1.isNull()) {
    fillFromArray(docGui1, "ratesource", out.rateSource,
                  sizeof(out.rateSource));
  }

  const JsonObject docGui2 = docGui[2];
  if (!docGui2.isNull()) {
    fillFromArray(docGui2, "animated", out.animated, sizeof(out.animated));
  }

  out.valid = (out.fundingSource[0] != '\0') || (out.rateSource[0] != '\0') ||
              (out.animated[0] != '\0');
  return out.valid;
}

bool ConfigService::saveGuiConfig(fs::FS &fs, const char *path,
                                  const GuiConfig &in) {
  DynamicJsonDocument docGui(2400);

  JsonObject docGui0 = docGui.createNestedObject();
  docGui0["name"] = "fundingsource";
  JsonArray valuesFundingSource = docGui0.createNestedArray("value");
  valuesFundingSource.add("Blink");
  valuesFundingSource.add("LNbits");
  valuesFundingSource.add("Flash");
  docGui0["checked"] = (strcmp(in.fundingSource, "Blink") == 0)   ? 1
                       : (strcmp(in.fundingSource, "Flash") == 0) ? 3
                                                                  : 2;

  JsonObject docGui1 = docGui.createNestedObject();
  docGui1["name"] = "ratesource";
  JsonArray valuesRateSource = docGui1.createNestedArray("value");
  valuesRateSource.add("CoinGecko");
  valuesRateSource.add("ExchangeApi");
  valuesRateSource.add("CoinYEP");
  valuesRateSource.add("Kraken");
  if (in.rateSource && (strcmp(in.rateSource, "ExchangeApi") == 0))
    docGui1["checked"] = 2;
  else if (in.rateSource && strcmp(in.rateSource, "CoinYEP") == 0)
    docGui1["checked"] = 3;
  else if (in.rateSource && strcmp(in.rateSource, "Kraken") == 0)
    docGui1["checked"] = 4;
  else
    docGui1["checked"] = 1;  // CoinGecko is default for unknown/empty

  JsonObject docGui2 = docGui.createNestedObject();
  docGui2["name"] = "animated";
  JsonArray valuesEnableAnim = docGui2.createNestedArray("value");
  valuesEnableAnim.add("No");
  valuesEnableAnim.add("Yes");
  docGui2["checked"] = (strcmp(in.animated, "No") == 0) ? 1 : 2;

  File guiFile = fs.open(path, "w");
  if (!guiFile) {
    return false;
  }

  serializeJson(docGui, guiFile);
  guiFile.close();
  return true;
}

#if FIAT_HAS_AUTOCONNECT
bool ConfigService::loadAuxConfig(fs::FS &fs, const char *path,
                                  AutoConnectAux &aux,
                                  std::initializer_list<const char *> keys) {
  File param = fs.open(path, "r");
  if (!param) {
    return false;
  }
  std::vector<String> names;
  names.reserve(keys.size());
  for (auto k : keys) {
    names.emplace_back(String(k));
  }
  aux.loadElement(param, names);
  param.close();
  return true;
}

bool ConfigService::saveAuxConfig(fs::FS &fs, const char *path,
                                  AutoConnectAux &aux,
                                  std::initializer_list<const char *> keys,
                                  String &echoOut) {
  File param = fs.open(path, "w");
  if (!param) {
    return false;
  }

  std::vector<String> names;
  names.reserve(keys.size());
  for (auto k : keys) {
    names.emplace_back(String(k));
  }
  aux.saveElement(param, names);
  param.close();

  // Reload to build echo string for portal
  File echoFile = fs.open(path, "r");
  if (echoFile) {
    echoOut = echoFile.readString();
    echoFile.close();
  }
  return true;
}
#endif // FIAT_HAS_AUTOCONNECT

static String csvField(const String &csv, int index) {
  int startPos = 0;
  int currentIndex = 0;

  while (currentIndex < index) {
    int commaPos = csv.indexOf(',', startPos);
    if (commaPos == -1) {
      return String("");
    }
    startPos = commaPos + 1;
    currentIndex++;
  }

  int commaPos = csv.indexOf(',', startPos);
  if (commaPos == -1) {
    return csv.substring(startPos);
  }
  return csv.substring(startPos, commaPos);
}

static void splitCsvToInts(const String &csv, std::vector<int> &out) {
  out.clear();
  int startPos = 0;
  int commaPos = csv.indexOf(',', startPos);

  while (commaPos != -1) {
    String value = csv.substring(startPos, commaPos);
    out.push_back(value.toInt());
    startPos = commaPos + 1;
    commaPos = csv.indexOf(',', startPos);
  }
  if (startPos < (int)csv.length()) {
    String value = csv.substring(startPos);
    out.push_back(value.toInt());
  }
}

bool ConfigService::loadFirst(fs::FS &fs, const char *path, FirstConfig &out) {
  File f = fs.open(path, "r");
  if (!f) {
    return false;
  }
  DynamicJsonDocument doc(2400);
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  if (error) {
    return false;
  }

  const JsonObject doc0 = doc[0];
  strlcpy(out.blinkApiKey, doc0["value"] | "", sizeof(out.blinkApiKey));

  const JsonObject doc1 = doc[1];
  strlcpy(out.blinkWalletId, doc1["value"] | "", sizeof(out.blinkWalletId));

  const JsonObject doc2 = doc[2];
  const String lnurlATM = String(doc2["value"] | "");
  strlcpy(out.baseUrl, csvField(lnurlATM, 0).c_str(), sizeof(out.baseUrl));
  strlcpy(out.secret, csvField(lnurlATM, 1).c_str(), sizeof(out.secret));
  strlcpy(out.currencyATM, csvField(lnurlATM, 2).c_str(),
          sizeof(out.currencyATM));

  const JsonObject doc3 = doc[3];
  strlcpy(out.adminKey, doc3["value"] | "", sizeof(out.adminKey));

  const JsonObject doc4 = doc[4];
  strlcpy(out.readKey, doc4["value"] | "", sizeof(out.readKey));

  const JsonObject doc5 = doc[5];
  strlcpy(out.currencyLabel, doc5["value"] | "", sizeof(out.currencyLabel));

  const JsonObject doc6 = doc[6];
  const String billmech = String(doc6["value"] | "");
  splitCsvToInts(billmech, out.billMech);

  const JsonObject doc7 = doc[7];
  out.maxAmount = String(doc7["value"] | "").toInt();

  const JsonObject doc8 = doc[8];
  out.charge = String(doc8["value"] | "").toFloat();

  out.valid = out.currencyLabel[0] != '\0';
  return out.valid;
}

bool ConfigService::updateFirstBlinkApiKey(fs::FS &fs, const char *path,
                                           const char *apiKey) {
  DynamicJsonDocument doc(2400);

  File f = fs.open(path, "r");
  bool loaded = false;
  if (f) {
    loaded = (deserializeJson(doc, f) == DeserializationError::Ok);
    f.close();
  }

  if (!loaded || !doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) {
    // No usable file yet - create the minimal positional layout loadFirst
    // expects (same order as the web flasher's makeFirstJson).
    doc.clear();
    JsonArray arr = doc.to<JsonArray>();
    static const char *const names[] = {
        "blinkapikey", "blinkwalletid", "lnurl",    "adminkey", "readkey",
        "currencyOne", "billmech",      "maxamount", "charge1"};
    for (auto name : names) {
      JsonObject o = arr.createNestedObject();
      o["name"] = name;
      o["type"] = "ACInput";
      o["value"] = "";
    }
  }

  doc[0]["value"] = apiKey;

  // Write to a temp file and only replace the original on a fully verified
  // write, so a power loss mid-write can't corrupt /first.json and brick the
  // config (this path stores the spending API key).
  const size_t expected = measureJson(doc);
  const String tmpPath = String(path) + ".tmp";

  File out = fs.open(tmpPath.c_str(), "w");
  if (!out) {
    return false;
  }
  const size_t written = serializeJson(doc, out);
  out.close();

  if (written != expected) {
    fs.remove(tmpPath.c_str());
    return false;
  }

  fs.remove(path); // SPIFFS rename fails if the destination exists
  if (!fs.rename(tmpPath.c_str(), path)) {
    fs.remove(tmpPath.c_str());
    return false;
  }
  return true;
}

bool ConfigService::loadSecond(fs::FS &fs, const char *path,
                               SecondConfig &out) {
  File f = fs.open(path, "r");
  if (!f) {
    return false;
  }
  DynamicJsonDocument doc(2400);
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  if (error) {
    return false;
  }

  const JsonObject doc0 = doc[0];
  strlcpy(out.currencyLabel, doc0["value"] | "", sizeof(out.currencyLabel));

  const JsonObject doc1 = doc[1];
  const String lnurlATM = String(doc1["value"] | "");
  strlcpy(out.baseUrl, csvField(lnurlATM, 0).c_str(), sizeof(out.baseUrl));
  strlcpy(out.secret, csvField(lnurlATM, 1).c_str(), sizeof(out.secret));
  strlcpy(out.currencyATM, csvField(lnurlATM, 2).c_str(),
          sizeof(out.currencyATM));

  const JsonObject doc2 = doc[2];
  const String billmech = String(doc2["value"] | "");
  splitCsvToInts(billmech, out.billMech);

  const JsonObject doc3 = doc[3];
  out.maxAmount = String(doc3["value"] | "").toInt();

  const JsonObject doc4 = doc[4];
  out.charge = String(doc4["value"] | "").toFloat();

  out.valid = out.currencyLabel[0] != '\0';
  return out.valid;
}

bool ConfigService::loadThird(fs::FS &fs, const char *path, ThirdConfig &out) {
  File f = fs.open(path, "r");
  if (!f) {
    return false;
  }
  DynamicJsonDocument doc(2400);
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  if (error) {
    return false;
  }

  const JsonObject doc0 = doc[0];
  strlcpy(out.currencyLabel, doc0["value"] | "", sizeof(out.currencyLabel));

  const JsonObject doc1 = doc[1];
  const String lnurlATM = String(doc1["value"] | "");
  strlcpy(out.baseUrl, csvField(lnurlATM, 0).c_str(), sizeof(out.baseUrl));
  strlcpy(out.secret, csvField(lnurlATM, 1).c_str(), sizeof(out.secret));
  strlcpy(out.currencyATM, csvField(lnurlATM, 2).c_str(),
          sizeof(out.currencyATM));

  const JsonObject doc2 = doc[2];
  const String billmech = String(doc2["value"] | "");
  splitCsvToInts(billmech, out.billMech);

  const JsonObject doc3 = doc[3];
  out.maxAmount = String(doc3["value"] | "").toInt();

  const JsonObject doc4 = doc[4];
  out.charge = String(doc4["value"] | "").toFloat();

  out.valid = out.currencyLabel[0] != '\0';
  return out.valid;
}

// Built-in LNURL-withdraw proxies, in priority order.
static const char *const kDefaultProxyEndpoints[] = {
    "https://api.lnbc.sk/v1/lnurl",
    "https://api2.lnbc.sk/v1/lnurl",
};

String ConfigService::defaultProxyEndpoints(const char *sep) {
  String out;
  for (auto url : kDefaultProxyEndpoints) {
    if (out.length()) out += sep;
    out += url;
  }
  return out;
}

// Accept only absolute https URLs with a non-empty host ([A-Za-z0-9.-],
// optional :port) that fit DeviceState::proxyEndpoints. Mirrors
// isValidProxyUrl() in web-flasher/src/main.js.
static bool isUsableProxyUrl(const String &url) {
  if (!url.startsWith("https://") || url.length() <= 10 ||
      url.length() >= sizeof(DeviceState::proxyEndpoints[0]) ||
      url.indexOf(' ') >= 0) {
    return false;
  }
  const int hostStart = 8; // strlen("https://")
  int i = hostStart;
  while (i < (int)url.length() &&
         (isalnum((unsigned char)url[i]) || url[i] == '.' || url[i] == '-')) {
    i++;
  }
  const int hostLen = i - hostStart;
  if (hostLen == 0 || url[hostStart] == '.' || url[hostStart] == '-' ||
      url[i - 1] == '.' || url[i - 1] == '-') {
    return false;
  }
  if (i < (int)url.length() && url[i] == ':') {
    const int portStart = ++i;
    while (i < (int)url.length() && isdigit((unsigned char)url[i])) i++;
    if (i == portStart) return false;
  }
  return i == (int)url.length() || url[i] == '/' || url[i] == '?' ||
         url[i] == '#';
}

typedef char ProxyUrl[sizeof(DeviceState::proxyEndpoints[0])];

static void addProxyEndpoint(ProxyUrl *list, size_t &count,
                             const String &url) {
  if (count >= DeviceState::kMaxProxyEndpoints || !isUsableProxyUrl(url)) {
    return;
  }
  for (size_t i = 0; i < count; i++) {
    if (url == list[i]) return;
  }
  strlcpy(list[count++], url.c_str(), sizeof(ProxyUrl));
}

bool ConfigService::loadProxyConfig(fs::FS &fs, const char *path,
                                    DeviceState &ds) {
  memset(ds.proxyEndpoints, 0, sizeof(ds.proxyEndpoints));
  ds.proxyEndpointCount = 0;

  File f = fs.open(path, "r");
  if (f) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    if (!error) {
      for (JsonVariant v : doc["endpoints"].as<JsonArray>()) {
        String url = v.as<String>();
        url.trim();
        addProxyEndpoint(ds.proxyEndpoints, ds.proxyEndpointCount, url);
      }
    }
  }

  ds.proxyFromFile = ds.proxyEndpointCount > 0;
  if (!ds.proxyFromFile) {
    for (auto url : kDefaultProxyEndpoints) {
      addProxyEndpoint(ds.proxyEndpoints, ds.proxyEndpointCount, url);
    }
  }
  return ds.proxyFromFile;
}

bool ConfigService::saveProxyConfig(fs::FS &fs, const char *path,
                                    const String &list) {
  ProxyUrl urls[DeviceState::kMaxProxyEndpoints] = {{0}};
  size_t count = 0;
  int start = 0;
  const int len = list.length();
  while (start <= len) {
    int end = start;
    while (end < len && list[end] != '\n' && list[end] != '\r' &&
           list[end] != ',') {
      end++;
    }
    String url = list.substring(start, end);
    url.trim();
    if (url.length()) addProxyEndpoint(urls, count, url);
    start = end + 1;
  }

  if (count == 0) {
    fs.remove(path);
    return true;
  }

  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.createNestedArray("endpoints");
  for (size_t i = 0; i < count; i++) {
    arr.add((const char *)urls[i]);
  }

  File f = fs.open(path, "w");
  if (!f) {
    return false;
  }
  serializeJson(doc, f);
  f.close();
  return true;
}
