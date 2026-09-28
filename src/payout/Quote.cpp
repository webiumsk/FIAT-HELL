#include "payout/Quote.h"

#include <math.h>

static bool feeInRange(double fee) {
  return fee == fee && fee >= 0.0 && fee <= 100.0;
}

static bool priceUsable(double price) {
  return price == price && price > 0.0 && price != INFINITY;
}

static Quote fromRaw(double raw) {
  Quote quote;
  quote.ok = false;
  quote.sats = 0;
  if (!(raw == raw) || raw <= 0.0 || raw >= (double)INT64_MAX) {
    return quote;
  }
  const long long sats = llround(raw);
  if (sats <= 0) {
    return quote;
  }
  quote.ok = true;
  quote.sats = (int64_t)sats;
  return quote;
}

Quote quoteSats(int64_t cents, double btcPrice, double feePercent) {
  if (cents <= 0 || !priceUsable(btcPrice) || !feeInRange(feePercent)) {
    Quote quote;
    quote.ok = false;
    quote.sats = 0;
    return quote;
  }
  const double beforeFee =
      (static_cast<double>(cents) / 100.0) / btcPrice * 1e8;
  return fromRaw(beforeFee * (100.0 - feePercent) / 100.0);
}

Quote quoteMixedSats(const MixedLeg *legs, int count) {
  Quote quote;
  quote.ok = false;
  quote.sats = 0;
  if (legs == nullptr || count <= 0) {
    return quote;
  }
  double sum = 0.0;
  bool any = false;
  for (int i = 0; i < count; i++) {
    if (legs[i].cents == 0) {
      continue;
    }
    if (legs[i].cents < 0 || !priceUsable(legs[i].btcPrice) ||
        !feeInRange(legs[i].feePercent)) {
      return quote;
    }
    const double beforeFee =
        (static_cast<double>(legs[i].cents) / 100.0) / legs[i].btcPrice * 1e8;
    sum += beforeFee * (100.0 - legs[i].feePercent) / 100.0;
    any = true;
  }
  if (!any) {
    return quote;
  }
  return fromRaw(sum);
}
