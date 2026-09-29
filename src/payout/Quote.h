#pragma once

#include <stdint.h>

// Fiat-to-sats quote shared by both boards. No Arduino types, so host tests
// can compile it. Prices are fiat units per 1 BTC. Fees are percent, 0 to 100.
// A quote fails when a used price is missing, a fee is out of range, or the
// rounded result is not a positive number of sats.

struct Quote {
  bool ok;
  int64_t sats;
};

struct MixedLeg {
  int64_t cents;
  double btcPrice;
  double feePercent;
};

Quote quoteSats(int64_t cents, double btcPrice, double feePercent);
Quote quoteMixedSats(const MixedLeg *legs, int count);
