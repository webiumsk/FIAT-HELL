#pragma once

#include <stdint.h>

// Points at the BOLT11 string, skipping whitespace and a lightning: prefix.
// Returns nullptr when invoice is nullptr.
const char *bolt11PaymentRequest(const char *invoice);

// Reads the BOLT11 human-readable amount (lnbc / lntb / lnbcrt, optional
// m/u/n/p multiplier). Amountless and non-whole-sat amounts are rejected.
// The bech32 checksum is not checked; callers still pay only after this
// amount matches the quote.
bool bolt11AmountSats(const char *invoice, int64_t *satsOut);
