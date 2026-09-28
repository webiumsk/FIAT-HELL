#pragma once

#include "DeviceState.h"
#include "SessionState.h"

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
 * Every call opens its own HTTPS connection (HttpsSession), so the functions
 * are safe to call from the S3 price task and the main loop at the same time.
 */
namespace FundingService {

bool isGaloy(const char *fundingSource);
const char *galoyEndpoint(const char *fundingSource);

/**
 * Which wallet currency the payout must run from. Flash's BTC wallet is
 * external (non-custodial Breez, isExternal=true) — the server has no keys,
 * returns balance=null and cannot pay from it — so Flash uses the custodial
 * USD "Cash" wallet (balance in cents, USD→sats conversion happens at payout).
 * Blink keeps the BTC wallet.
 */
const char *galoyWalletCurrency(const char *fundingSource);

/**
 * Fetch wallets via `query me` and pick the one matching walletCurrency
 * ("BTC" unless a hedged fiat wallet is desired). On success stores the
 * wallet id into ds.blinkwalletid and the balance into ss.balanceSats.
 * fiatBalance conversion is left to the caller (board-specific math).
 */
bool fetchGaloyBalance(DeviceState &ds, SessionState &ss,
                       const char *walletCurrency = "BTC");

/**
 * Query the scopes of the configured Galoy API key
 * (query authorization { authorization { scopes } }). Fills scopesOut with a
 * comma-separated list like "READ,RECEIVE,WRITE". A key without WRITE can
 * read the balance but every payout fails with AuthorizationError.
 */
bool fetchGaloyAuthorization(const DeviceState &ds, char *scopesOut,
                             size_t scopesLen);

/**
 * Reachability check for an LNbits funding wallet: GET /api/v1/wallet with
 * the read key. balanceSatsOut receives the reported balance when non-null.
 */
bool checkLNbitsWallet(const DeviceState &ds, long *balanceSatsOut);

/**
 * Pay a BOLT11 invoice via lnInvoicePaymentSend from the configured wallet.
 * Returns true only when the backend reports SUCCESS, PENDING (payment in
 * flight) or ALREADY_PAID — callers must treat false as a failed payout.
 * walletIdOverride, when non-empty, is used instead of ds.blinkwalletid —
 * for callers that snapshot the id under a lock (S3 background task).
 */
bool payInvoice(const DeviceState &ds, const char *invoice,
                const char *walletIdOverride = nullptr);

/**
 * Ask the LNURL-withdraw proxy for a withdraw QR worth amountSats.
 * On success fills ss.lnURLgen, ss.modifiedLnURLgen and ss.callback.
 */
bool requestLnurlWithdraw(SessionState &ss, long amountSats);

/**
 * Poll the proxy callback URL for the customer's BOLT11 invoice.
 * Returns true and fills ss.boltInvoice once the wallet submitted one.
 */
bool pollBoltInvoice(SessionState &ss);

// Short text for the payment-failed screen. Empty until a payout fails.
const char *payoutFailureReason();
void setPayoutFailure(const char *reason);
void clearPayoutFailure();

// True when polling cannot succeed (the invoice will not fit). A normal
// "not ready yet" poll leaves this false.
bool payoutIsFatal();

// Latest poll diagnostic, for the timeout screen.
const char *lastPollNote();

} // namespace FundingService
