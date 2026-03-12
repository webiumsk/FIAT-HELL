/**
 * @file DeviceState.h
 * @brief Device state structure for persistent device configuration
 *
 * This structure contains all persistent device configuration that changes
 * infrequently (e.g., currency settings, API keys, limits, etc.)
 */

#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <stddef.h>
#include <string.h>
#include <vector>

/**
 * @brief Persistent device configuration state
 *
 * Contains all configuration that persists across reboots and changes
 * infrequently. This includes currency settings, API credentials, limits,
 * and bill acceptor configuration.
 */
struct DeviceState {
  // Currency configuration (3 currencies supported)
  char currencyOne[64] = {0};
  char currencyTwo[64] = {0};
  char currencyThree[64] = {0};
  char currencyATM[64] = {0};
  char currencyATM2[64] = {0};
  char currencyATM3[64] = {0};

  // LNbits configuration
  char lnbitsURL[256] = {0};
  char adminkey[256] = {0};
  char readkey[256] = {0};

  // Blink configuration
  char blinkapikey[128] = {0};
  char blinkwalletid[128] = {0};

  // ATM display configuration
  char atmtitle[64] = "FIAT HELL";
  char atmsubtitle[128] = {0};
  char atmdesc[256] = {0};
  char password[64] = "changeme"; // WiFi AP password

  // Funding source configuration
  char fundingSourceBuffer[100] = {0};
  char rateSourceBuffer[100] = {0};
  char enableAnimBuffer[100] = {0};

  // LNbits base URLs and secrets (3 currencies)
  char baseURLATM1[256] = {0};
  char baseURLATM2[256] = {0};
  char baseURLATM3[256] = {0};
  char secretATM1[256] = {0};
  char secretATM2[256] = {0};
  char secretATM3[256] = {0};

  // LNURL configuration (3 currencies)
  char lnurl[1024] = {0};
  char lnurl2[1024] = {0};
  char lnurl3[1024] = {0};

  // Limits and charges (3 currencies)
  float maxamount = 100.0f;
  float maxamount2 = 0.0f;
  float maxamount3 = 0.0f;
  float charge1 = 0.0f;
  float charge2 = 0.0f;
  float charge3 = 0.0f;

  // Bill acceptor configuration
  std::vector<int> billAmountIntOne;
  std::vector<int> billAmountIntTwo;
  std::vector<int> billAmountIntThree;
  size_t originalSizeOne = 0;
  size_t originalSizeTwo = 0;
  size_t originalSizeThree = 0;

  /**
   * @brief Reset all fields to default values
   */
  void reset() {
    memset(currencyOne, 0, sizeof(currencyOne));
    memset(currencyTwo, 0, sizeof(currencyTwo));
    memset(currencyThree, 0, sizeof(currencyThree));
    memset(currencyATM, 0, sizeof(currencyATM));
    memset(currencyATM2, 0, sizeof(currencyATM2));
    memset(currencyATM3, 0, sizeof(currencyATM3));
    memset(lnbitsURL, 0, sizeof(lnbitsURL));
    memset(adminkey, 0, sizeof(adminkey));
    memset(readkey, 0, sizeof(readkey));
    memset(blinkapikey, 0, sizeof(blinkapikey));
    memset(blinkwalletid, 0, sizeof(blinkwalletid));
    memset(atmsubtitle, 0, sizeof(atmsubtitle));
    memset(atmdesc, 0, sizeof(atmdesc));
    strcpy(password, "changeme");
    memset(fundingSourceBuffer, 0, sizeof(fundingSourceBuffer));
    memset(rateSourceBuffer, 0, sizeof(rateSourceBuffer));
    memset(enableAnimBuffer, 0, sizeof(enableAnimBuffer));
    memset(baseURLATM1, 0, sizeof(baseURLATM1));
    memset(baseURLATM2, 0, sizeof(baseURLATM2));
    memset(baseURLATM3, 0, sizeof(baseURLATM3));
    memset(secretATM1, 0, sizeof(secretATM1));
    memset(secretATM2, 0, sizeof(secretATM2));
    memset(secretATM3, 0, sizeof(secretATM3));
    memset(lnurl, 0, sizeof(lnurl));
    memset(lnurl2, 0, sizeof(lnurl2));
    memset(lnurl3, 0, sizeof(lnurl3));
    maxamount = 100.0f;
    maxamount2 = 0.0f;
    maxamount3 = 0.0f;
    charge1 = 0.0f;
    charge2 = 0.0f;
    charge3 = 0.0f;
    billAmountIntOne.clear();
    billAmountIntTwo.clear();
    billAmountIntThree.clear();
    originalSizeOne = 0;
    originalSizeTwo = 0;
    originalSizeThree = 0;
  }
};

#endif // DEVICE_STATE_H
