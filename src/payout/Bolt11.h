#pragma once

#include <stdint.h>

// Reads the BOLT11 human-readable amount (lnbc / lntb / lnbcrt, optional
// m/u/n/p multiplier). Amountless and non-whole-sat amounts are rejected.
// The bech32 checksum is not checked; callers still pay only after this
// amount matches the quote.
bool bolt11AmountSats(const char *invoice, int64_t *satsOut);
