# Návrhy vylepšení – FIAT-HELL

Analýza celého projektu (stav k 2026-05-20). Problémy sú zoradené od kritických po menšie.

---

## 🔴 Kritické – bezpečnosť a spoľahlivosť

### 1. TLS certifikát nie je overovaný nikde

**Súbor:** `src/main.cpp:917`, `src/PriceBalanceTask.cpp`, `otaDoAux` handler

Všade sa volá `secureClient.setInsecure()` alebo lokálne `client.setInsecure()`. Platby cez Blink aj LNbits prebiehajú bez overenia certifikátu – MITM útok môže interceptovať LNURL/invoice. Obzvlášť kritické pri OTA: útočník na sieti môže nahrať ľubovoľný firmware.

**Riešenie:** Pridať root CA certifikát pre `api.blink.sv`, `lnpay.eu` a prípadne LNbits inštanciu ako `const char[]` a volať `secureClient.setCACert(root_ca)`. Pre OTA je to obzvlášť dôležité.

---

### 2. API kľúč hardkódovaný v zdrojáku

**Súbor:** `src/main.cpp:258`

```cpp
const char *cuexApiKey = "3b71e5d431b2331acb65f2d484d423e5";
```

API kľúč je commitnutý priamo v kóde a endpoint (`cuexConversionAPI`) sa pritom ani nepoužíva v žiadnej aktívnej code path – kód je mŕtvy, ale kľúč zostáva v Git histórií.

**Riešenie:** Odstrániť `cuexApiKey`, `cuexConversionAPI` aj `alternativeConversionAPI` (cryptocompare) – všetky sú mŕtvy kód.

---

### 3. Memory leak v každej offline transakcii

**Súbor:** `src/main.cpp:3456–3460`

```cpp
byte *data = (byte *)calloc(strlen(url) * 2, sizeof(byte));   // nikdy free()
char *charLnurl = (char *)calloc(strlen(url) * 2, sizeof(byte)); // nikdy free()
```

`makeLNURL()` je volaná pri každej offline LNbits transakcii. Oba `calloc` bloky sú alokované ale nikdy uvoľnené. Na zariadení s limitovanou heap (napr. 200 kB voľnej) to po niekoľkých transakciách spôsobí crash.

**Riešenie:** Nahradiť lokálnymi stackovými buffermi (URL má max ~200 znakov, bech32-encoded výstup max ~450 znakov), alebo pridať `free(data); free(charLnurl);` pred každým `return` z funkcie.

---

### 4. Race condition: PriceBalanceTask píše do SessionState bez zámku

**Súbor:** `src/PriceBalanceTask.cpp:231–238` vs. `src/main.cpp`

FreeRTOS task drží mutex len počas zápisu do `g_sessionState`. Hlavná slučka (`loop()`) číta `sessionState.fiatValue`, `sessionState.fiatBalance` a `sessionState.fiatValue1/2/3` bez toho, aby vzala mutex. Na ESP32-S3 s dvomi jadrami to môže spôsobiť čítanie nekonzistentných hodnôt (napr. cena prepísaná na nulu počas výpočtu sats).

**Riešenie:** Hlavná slučka by mala brať `g_dataMutex` pred čítaním price/balance hodnôt, alebo použiť atomic kópiu do lokálnych premenných po `consumePriceBalanceDataReady()`.

---

## 🟠 Vážne – architektúra a udržateľnosť

### 5. `main.cpp` má 4169 riadkov

Celá logika – setup, loop, UI (7 obrazoviek), platobné toky, bill acceptor, OTA, konfiguračný portál, batéria – je v jedinom súbore. `src/services/` začal správnu dekompozíciu ale ďalej sa nerozvíjala.

**Návrh rozdelenia:**
- `src/BillAcceptor.cpp/h` – NV10 protokol (inhibit/uninhibit, channel setup)
- `src/LnurlService.cpp/h` – `getLNURL()`, `makeLNURL()`, `createLNURLWithdraw()`, `getBlinkLNURL()`
- `src/screens/MainScreen.cpp/h`, `screens/InsertMoneyScreen.cpp/h`, atď. – screeny presunuté zo `page*.h` headerov do vlastných `.cpp` súborov

---

### 6. ~45 `#define` makier namiesto referencií na štruktúry

**Súbor:** `src/main.cpp:130–244`

```cpp
#define bills sessionState.bills
#define total sessionState.total
// ... 40+ ďalších
```

Makrá skrývajú, kde skutočne žijú dáta, znemožňujú krokový debugging (makro sa neobjaví vo watchpointoch) a spôsobujú najhorší prípad v kóde: `#undef`/`#define` makier v strede `loop()` (riadky 4049–4076) kvôli konfliktu názvov pri mixed-currency path. Toto je krehký anti-pattern.

**Riešenie:** Odstrániť makrá postupne; volať priamo `sessionState.bills`, `sessionState.total`. V dlhých funkciách kde sa to opakuje mnohokrát, použiť referencie: `auto &ss = *sessionStatePtr;`.

---

### 7. Kód načítania konfigurácie zduplikovaný

**Súbory:** `src/main.cpp:945–1162` (v `setup()`) a `src/main.cpp:632–748` (`reloadRuntimeConfigFromFlash()`)

Rovnaký blok – načítanie 5 JSON súborov + aplikovanie do `DeviceState` – existuje dvakrát s minimálnymi rozdielmi. Keď sa zmení formát konfigurácie, treba aktualizovať na dvoch miestach.

**Riešenie:** `setup()` by mal volať `reloadRuntimeConfigFromFlash()` namiesto vlastného kódu.

---

### 8. Extrakcia `lnbitsURL` z `baseURLATM1` zduplikovaná 3×

**Súbor:** `src/main.cpp:599–618`, `src/main.cpp:718–734`, `src/main.cpp:4057–4072`

Logika "nájdi tretí lomítko a orež reťazec" sa opakuje trikrát. Aj chyba v nej je zduplikovaná (napr. neošetrený `strncpy` bez null terminácie na jednom mieste).

**Riešenie:** Extrahovať do `static void extractBaseUrl(const char *fullUrl, char *out, size_t outLen)` v `ConfigService`.

---

### 9. Callbacks AutoConnect aux načítavajú config dvakrát v tom istom handleri

**Súbor:** `src/main.cpp:1047–1060`, `src/main.cpp:1081–1091`, `src/main.cpp:1113–1124`

```cpp
firstAux.on([](AutoConnectAux &aux, PageArgument &arg) {
    configService.loadAuxConfig(...);           // vždy
    if (portal.where() == "/first") {
        configService.loadAuxConfig(...);       // znova, tie isté parametre
    }
    return String();
});
```

Každý callback načítava súbor dvakrát (raz bezpodmienečne, raz podmienečne). Zbytočná I/O záťaž.

---

### 10. `portal.join({...})` volaný dvakrát s rovnakým zoznamom

**Súbor:** `src/main.cpp:1450` (v `setup()`) a `src/main.cpp:3650` (v `startConfigPortal()`)

Dvojité registrovanie aux stránok môže spôsobiť, že AutoConnect zaregistruje handlery duplicitne.

---

## 🟡 Stredné – chyby a mŕtvy kód

### 11. `isLNbits()` má nedosiahnuteľný kód

**Súbor:** `src/main.cpp:1904–1912`

```cpp
bool isLNbits() {
  if (...) { return true; } else { return false; }
  Serial.print("isLNbits: ");   // NIKDY SA NEDOSIAHNE
  Serial.println(isLNbits());   // aj rekurzívne volanie!
}
```

---

### 12. Mŕtvy kód a zbytočné globálne premenné

| Položka | Súbor | Popis |
|---------|-------|-------|
| `getValue()` funkcia | `main.cpp:761` | Definovaná, nikde volaná |
| `isLoopReading` | `main.cpp:3696` | Nastavená vždy na `false`, nikdy sa nemení |
| `totalStr[64]` global | `main.cpp:184` | Alokovaná, nikdy použitá |
| `cuexConversionAPI`, `cuexApiKey` | `main.cpp:249–258` | Endpoint a kľúč, nikde nevyužité |
| `alternativeConversionAPI` | `main.cpp:259–260` | Nikde nevyužité |
| `firstPtr`, `secondPtr`, `thirdPtr` | `main.cpp:808–821` | `AutoConnectConfig*` objekty alokované ale nikde konfigurované ani použité (len `configPtr` sa skutočne používa) |

---

### 13. `format = true` flag ako compile-time premenná

**Súbor:** `src/main.cpp:8`

```cpp
bool format = false; // true for formatting FOSSA memory, use once, then make false and reflash
```

Ak niekto nastaví na `true` a zabudne, SPIFFS sa zformátuje pri každom boote (stratené nastavenia). Navyše sa SPIFFS formátuje aj cez `FlashFS` aj cez `SPIFFS` (`FORMAT_ON_FAIL`), čo je redundantné.

**Riešenie:** Odstrániť a formátovanie riešiť jednorazovým firmware buildom s `#define FACTORY_RESET`, alebo tlačidlom v portáli.

---

### 14. `FlashFS.begin()` + `SPIFFS.begin()` volaný dvakrát v `setup()`

**Súbor:** `src/main.cpp:938–939` a `src/main.cpp:1128–1129`

---

### 15. Mätúce pomenovanie: `balanceSats` v skutočnosti uchováva millisatoshi pre LNbits

**Súbor:** `src/main.cpp:2274–2276`, `src/PriceBalanceTask.cpp:155–157`

LNbits `/api/v1/wallet` endpoint vracia zostatok v **millisatoshi**. Premenná sa volá `balanceSats` ale konverzia delí `1e11` (= msat → BTC), nie `1e8` (= sat → BTC). Blink vracia skutočné satoshi (delí `1e8`). Táto nekonzistencia je skrytá v kóde a môže spôsobiť záhadné chyby pri zmene backend-u.

**Riešenie:** Premenovať na `balanceMsats` pri LNbits ceste, alebo normalizovať na satoshi hneď pri parsovaní.

---

### 16. `checkPriceCoinGecko()` v skutočnosti volá CoinYEP API

**Súbor:** `src/main.cpp:2083–2121`

Funkcia sa volá `checkPriceCoinGecko` ale volá `coinyepConversionAPI`. Komentár to vysvetľuje ("kept for backward compatibility") ale mätúce pre každého, kto číta kód. Podobne, v `checkPrice()` existuje vetva pre `"Coingecko"` (malé g) aj `"CoinGecko"` (veľké G).

---

### 17. `DynamicJsonDocument(16384)` pre ExchangeApi

**Súbor:** `src/main.cpp:2131`, `src/PriceBalanceTask.cpp:92`

ExchangeApi JSON obsahuje kurzy pre všetky kryptomeny – sťahuje sa cez HTTP, parsuje sa do 16 kB buffra na heape. V skutočnosti potrebujeme len jeden kľúč (`btc.<currency>`). Pri veľkých odpovediach môže parsovanie zlyhať a 16 kB alokácia zakaždým fragmentuje heap.

**Riešenie:** Použiť `StreamingDeserializationFilter` (ArduinoJson filter) na vytiahnutie len potrebného kľúča.

---

### 18. Hardcoded `gpio 11` bez komentára

**Súbor:** `src/main.cpp:1594`

```cpp
digitalWrite(11, LOW);
```

Bez `#define` ani komentára. Na ESP32-S3 je GPIO11 interné (flash), jeho použitie ako výstup môže spôsobiť problémy.

---

## 🟢 Menšie vylepšenia a UX

### 19. Globálne buffery `buffer[32]` a `Buf[200]` sú tienené lokálnymi premennými

**Súbor:** `src/main.cpp:227`, `src/main.cpp:3453`

Globálny `char buffer[32]` je deklarovaný globálne ale v mnohých funkciách je tienený lokálnym `char buffer[32]`. Globálny `char Buf[200]` je tienený lokálnym `char Buf[200]` v `makeLNURL()`. Toto spôsobuje zmätok pri čítaní kódu. Oba globálne buffery možno odstrániť – funkcie majú vlastné lokálne kópie.

---

### 20. Batériový ADC číta iba jeden vzorek

**Súbor:** `src/main.cpp:321–330`

ESP32 ADC je notoricky šumivý (±100–200 mV odchýlka). Jedno meranie dáva nestabilné hodnoty – indikátor "skoká" medzi hodnotami. Triviálna oprava: priemerovanie 4–8 vzoriek.

```cpp
// Navrhovaná zmena:
int raw = 0;
for (int i = 0; i < 8; i++) raw += analogRead(BATTERY_ADC_GPIO);
raw /= 8;
```

---

### 21. Timeout transakcie neexistuje

Ak zákazník vloží bankovku a odíde bez toho, aby ťukol na obrazovku, ATM zostane zaseknutý v stave `UI_INSERTING_MONEY` navždy (bill acceptor zostane deaktivovaný). Stav `UI_WAITING_FOR_BLINK_INVOICE` má timeout 5 minút (reštart), ale `UI_INSERTING_MONEY` nie.

**Riešenie:** Pridať timeout do `handleUiStateMachine()` pre `UI_INSERTING_MONEY` – napr. ak 10 minút bez aktivity, reštartovať zariadenie.

---

### 22. `MAX_MIXED_EUR = 100.0f` nie je konfigurovateľný

**Súbor:** `src/main.cpp:3255`

Maximum pre mixed-currency mode je natvrdo 100 EUR. Malo by byť nastaviteľné cez portál (napr. ako globálny limit ATM).

---

### 23. "ATM.LNPAY.EU" URL hardcódovaný v logo screene

**Súbor:** `src/main.cpp:1651`

```cpp
String LVGL_ATMURL = "ATM.LNPAY.EU";
```

Malo by byť konfigurovateľné cez portál (napr. pole `atmurl`) alebo odvodené z `lnbitsURL`.

---

### 24. Chybové stavy nie sú zobrazené používateľovi

Všetky chyby (HTTP timeout, JSON parse error, prázdny LNURL) sú vypísané len do Serial monitora. Používateľ na display nevidí nič. Minimálne by sa mal objaviť error message na QR screene ak `getLNURL()` vráti prázdny `lnURLgen`.

---

### 25. `setup()` funkcia má ~800 riadkov

Registrácia 12 AutoConnect aux stránok, načítanie konfigurácie, inicializácia periférií – všetko v jednej funkcii. Každý logický blok by mal byť vlastná funkcia (`initDisplay()`, `initPortalPages()`, `loadAllConfig()`, `initPeripherals()`).

---

### 26. Logo wait timeout je 2000ms (2 sekundy)

**Súbor:** `src/main.cpp:3735`

Používatelia, ktorí reagujú pomalšie, nemusia stihnúť ťuknúť pre vstup do portálu. 5 sekúnd by bolo praktickejšie (pôvodný kód mal `5000`).

---

### 27. PriceBalanceTask nemá watchdog / restart pri zlyhaní HTTP

Ak WiFi spojenie prepadne počas HTTP requestu v `priceBalanceTaskFunc`, task môže zastihnúť `xQueueReceive` a jednoducho preskočiť ďalší request bez loggovania. Chýba timeout + logovanie zlyhania per-requesty.

---

### 28. `Buf[200]` globálny buffer (kryptooperácie)

**Súbor:** `src/main.cpp:106`

```cpp
char Buf[200];  // Buffer for the encrypted data
```

Komentár hovorí "pre encrypted data" ale buffer je použitý aj v `makeLNURL()` pre URL string (lokálne shadowing). Premenovanie na `xorPayloadBuf` a urobenie z neho lokálneho v `xor_encrypt` by eliminovalo zmätok.

---

## Sumár priorít

| Priorita | Počet | Príklady |
|----------|-------|---------|
| 🔴 Kritické | 4 | TLS bez overenia, API kľúč v kóde, memory leak, race condition |
| 🟠 Vážne | 6 | 4169-riadkový main.cpp, 45 makier, duplikovaný config kód |
| 🟡 Stredné | 8 | Mŕtvy kód, misleading meno, ExchangeApi 16kB buffer |
| 🟢 Menšie | 9 | Timeout transakcie, ADC averaging, hardcoded reťazce |

**Odporúčané prvé kroky:**
1. Odstrániť `cuexApiKey` z kódu (aj z Git histórie ideálne `git filter-repo`)
2. Opraviť `calloc` leak v `makeLNURL()` (trivial fix)
3. Pridať TLS certifikát aspoň pre OTA server
4. Zlúčiť duplicitný config loading kód
5. Odstrániť mŕtvy kód (cca 50 riadkov ľahko)
