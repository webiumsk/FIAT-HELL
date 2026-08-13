#pragma once

#include "DeviceState.h"
#include "SessionState.h"
#include <HTTPClient.h>

/**
 * Shared funding-source client used by both boards.
 *
 * Galoy half: Blink (api.blink.sv) and Flash (api.flashapp.me) run the same
 * Galoy GraphQL API and X-API-KEY auth; only the endpoint differs. Credentials
 * live in DeviceState::blinkapikey / blinkwalletid regardless of which of the
 * two is selected.
 *
 * Proxy half: the LNURL-withdraw QR shown to the customer is produced by the
 * lnbc.sk proxy (lnurlproxy.me fallback); the wallet backend only pays the
 * resulting BOLT11 invoice.
 *
 * The HTTPClient is passed in by the caller: the S3 board calls this from a
 * FreeRTOS task with its own client while the main loop owns another.
 */
namespace FundingService {

bool isGaloy(const char *fundingSource);
const char *galoyEndpoint(const char *fundingSource);

/**
 * Fetch wallets via `query me` and pick the one matching walletCurrency
 * ("BTC" unless a hedged fiat wallet is desired). On success stores the
 * wallet id into ds.blinkwalletid and the balance into ss.balanceSats.
 * fiatBalance conversion is left to the caller (board-specific math).
 */
bool fetchGaloyBalance(HTTPClient &http, DeviceState &ds, SessionState &ss,
                       const char *walletCurrency = "BTC");

/**
 * Pay a BOLT11 invoice via lnInvoicePaymentSend from the configured wallet.
 * Returns true only when the backend reports SUCCESS, PENDING (payment in
 * flight) or ALREADY_PAID — callers must treat false as a failed payout.
 */
bool payInvoice(HTTPClient &http, const DeviceState &ds, const char *invoice);

/**
 * Ask the LNURL-withdraw proxy for a withdraw QR worth amountSats.
 * On success fills ss.lnURLgen, ss.modifiedLnURLgen and ss.callback.
 */
bool requestLnurlWithdraw(HTTPClient &http, SessionState &ss, long amountSats);

/**
 * Poll the proxy callback URL for the customer's BOLT11 invoice.
 * Returns true and fills ss.boltInvoice once the wallet submitted one.
 */
bool pollBoltInvoice(HTTPClient &http, SessionState &ss);

} // namespace FundingService
