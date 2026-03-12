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
  docGui0["checked"] = (strcmp(in.fundingSource, "Blink") == 0) ? 1 : 2;

  JsonObject docGui1 = docGui.createNestedObject();
  docGui1["name"] = "ratesource";
  JsonArray valuesRateSource = docGui1.createNestedArray("value");
  valuesRateSource.add("CoinGecko");
  valuesRateSource.add("ExchangeApi");
  valuesRateSource.add("CoinYEP");
  valuesRateSource.add("Kraken");
  if (strcmp(in.rateSource, "CoinGecko") == 0 || strcmp(in.rateSource, "Coingecko") == 0)
    docGui1["checked"] = 1;
  else if (strcmp(in.rateSource, "ExchangeApi") == 0)
    docGui1["checked"] = 2;
  else if (strcmp(in.rateSource, "CoinYEP") == 0)
    docGui1["checked"] = 3;
  else
    docGui1["checked"] = 4;

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
