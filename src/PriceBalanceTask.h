/**
 * @file PriceBalanceTask.h
 * @brief Background task for fetching price and balance without blocking the
 * main loop.
 */

#ifndef PRICE_BALANCE_TASK_H
#define PRICE_BALANCE_TASK_H

#include "DeviceState.h"
#include "SessionState.h"
#include <stdbool.h>
#include <stdint.h>

enum PriceBalanceRequest {
  PBR_NONE = 0,
  PBR_PERIODIC
};

/**
 * @brief Trigger a background fetch of price and balance.
 */
void triggerPriceBalanceFetch(PriceBalanceRequest req);

/**
 * @brief Check if a fetch has completed and data is ready for UI update.
 */
bool isPriceBalanceDataReady();

/**
 * @brief Consume the data-ready flag when a fetch has completed.
 */
bool consumePriceBalanceDataReady();

/**
 * @brief Create and start the background task. Call once from setup() after
 * deviceState and sessionState are initialized.
 */
void startPriceBalanceTask(DeviceState *ds, SessionState *ss);

/**
 * @brief Copy the cached Galoy wallet id under the task's data mutex.
 *
 * The background task rewrites deviceState.blinkwalletid during balance
 * fetches; payout code must snapshot it through here to avoid reading a
 * half-written id. Returns false when the task isn't running (caller should
 * fall back to reading deviceState directly — no concurrent writer exists
 * then).
 */
bool priceBalanceCopyWalletId(char *dst, size_t dstSize);

#endif // PRICE_BALANCE_TASK_H
