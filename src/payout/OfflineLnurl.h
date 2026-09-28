#pragma once

#include <stdint.h>

// LNbits offline LNURL has one amount field, and the wallet reads it as fiat
// cents of a single currency. Mixed-currency and Galoy (Blink/Flash) payouts
// cannot use that encoding.

enum class FundingKind { Lnbits, Galoy };

struct OfflinePayout {
  bool allowed;
  uint64_t centsAfterFee;
};

OfflinePayout offlineLnurlPayout(FundingKind kind, bool mixed, int64_t cents,
                                 double feePercent);
