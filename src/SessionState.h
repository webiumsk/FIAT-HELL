/**
 * @file SessionState.h
 * @brief Session state structure for runtime/transaction state
 *
 * This structure contains all runtime state that changes during operation,
 * including current transaction data, UI state, and payment flow state.
 */

#ifndef SESSION_STATE_H
#define SESSION_STATE_H

#include <stdint.h>
#include <string.h>

/**
 * @brief UI state machine states
 *
 * Note: Using plain enum (not enum class) for compatibility with existing code
 */
enum UiState {
  UI_IDLE,
  UI_LOGO_WAIT,
  UI_INSERTING_MONEY,
  UI_SHOWING_QR,
  UI_WAITING_FOR_TAP,
  UI_WAITING_FOR_BLINK_INVOICE,
  UI_THANK_YOU,
  UI_PAYMENT_ERROR
};

/**
 * @brief Runtime session and transaction state
 *
 * Contains all state that changes during operation, including:
 * - Current transaction data (bills, coins, total)
 * - Currently selected values (currency, limits, charges)
 * - Current market values (fiat value, balance)
 * - UI state machine state
 * - Payment flow state (QR codes, invoices, etc.)
 */
struct SessionState {
  // Current transaction state
  int bills = 0;
  float coins = 0.0f;
  float total = 0.0f;
  bool isInsertingMoney = false;

  // Mixed-currency: totals per currency in cents (e.g. 500 = 5.00 EUR)
  long totalCurrency1 = 0;
  long totalCurrency2 = 0;
  long totalCurrency3 = 0;
  long lastBillCents = 0;
  char lastBillCurrency[64] = {0};

  // Rates for all 3 currencies (BTC per 1 unit of fiat), for mixed conversion
  float fiatValue1 = 0.0f;
  float fiatValue2 = 0.0f;
  float fiatValue3 = 0.0f;

  // Currently selected values (from DeviceState, but cached for quick access)
  char currencySelected[64] = {0};
  float maxamountSelected = 100.0f;
  float chargeSelected = 0.0f;

  // Allowed bill acceptor channels for selected currency (for software filter)
  int allowedChannelStart = 0;
  int allowedChannelCount = 0;

  // Current active base URL and secret (selected from DeviceState)
  char baseURLATM[256] = {0};
  char secretATM[256] = {0};

  // Current market values
  float fiatValue = 0.0f;   // Current BTC price in selected fiat
  float fiatBalance = 0.0f; // Current balance in selected fiat
  long balanceSats = 0;     // Current balance in satoshis (USD cents when the
                            // funding wallet is Flash's USD Cash wallet)
  float btcUsdValue = 0.0f; // BTC price in USD (cross rate for USD wallets)

  // UI state machine
  UiState currentUiState = UI_IDLE;
  unsigned long stateEnterTime = 0;
  bool qrDebounceDone = false;

  // Payment flow state
  bool isBlinkFlow = false;
  unsigned long lastBlinkPollTime = 0;

  // QR code and payment data
  char qrData[256] = {0};
  char lnURLgen[1024] = {0};
  char modifiedLnURLgen[1024] = {0};
  char callback[1024] = {0};
  char boltInvoice[1024] = {0};
  char paymentRequest[1024] = {0};
  char payload[1024] = {0};

  // Temporary calculation values
  float tempCharge = 0.0f;
  long result = 0;

  // Periodic update timing
  unsigned long previousMillis = 0;
  bool initialCheck = true;

  /**
   * @brief Reset transaction state (for new transaction)
   */
  void resetTransaction() {
    bills = 0;
    coins = 0.0f;
    total = 0.0f;
    isInsertingMoney = false;
    totalCurrency1 = 0;
    totalCurrency2 = 0;
    totalCurrency3 = 0;
    lastBillCents = 0;
    memset(lastBillCurrency, 0, sizeof(lastBillCurrency));
    tempCharge = 0.0f;
    result = 0;
  }

  /**
   * @brief Reset payment flow state
   */
  void resetPaymentFlow() {
    isBlinkFlow = false;
    lastBlinkPollTime = 0;
    memset(qrData, 0, sizeof(qrData));
    memset(lnURLgen, 0, sizeof(lnURLgen));
    memset(modifiedLnURLgen, 0, sizeof(modifiedLnURLgen));
    memset(callback, 0, sizeof(callback));
    memset(boltInvoice, 0, sizeof(boltInvoice));
    memset(paymentRequest, 0, sizeof(paymentRequest));
    memset(payload, 0, sizeof(payload));
  }

  /**
   * @brief Reset UI state
   */
  void resetUI() {
    currentUiState = UI_IDLE;
    stateEnterTime = 0;
    qrDebounceDone = false;
  }
};

#endif // SESSION_STATE_H
