#include "payout/NoteMask.h"

#include <math.h>

bool walletBalanceSats(BalanceUnit unit, int64_t balance, double btcUsdPrice,
                       int64_t *satsOut) {
  if (satsOut == nullptr) {
    return false;
  }
  *satsOut = 0;
  if (balance <= 0) {
    return false;
  }
  int64_t sats = 0;
  switch (unit) {
  case BalanceUnit::Sats:
    sats = balance;
    break;
  case BalanceUnit::Msats:
    sats = balance / 1000;
    break;
  case BalanceUnit::UsdCents: {
    if (!(btcUsdPrice == btcUsdPrice) || btcUsdPrice <= 0.0 ||
        btcUsdPrice == INFINITY) {
      return false;
    }
    const double raw = ((double)balance / 100.0) / btcUsdPrice * 1e8;
    if (!(raw == raw) || raw >= (double)INT64_MAX) {
      return false;
    }
    sats = (int64_t)floor(raw);
    break;
  }
  }
  if (sats <= 0) {
    return false;
  }
  *satsOut = sats;
  return true;
}

uint16_t fittingNoteMask(const NoteChannel *channels, int count,
                         const MixedLeg legs[3], int64_t walletSats) {
  if (channels == nullptr || legs == nullptr || walletSats <= 0) {
    return 0;
  }
  const int limit = count < 16 ? count : 16;
  uint16_t mask = 0;
  for (int i = 0; i < limit; i++) {
    const NoteChannel channel = channels[i];
    if (channel.amount <= 0 || channel.leg < 0 || channel.leg > 2) {
      continue;
    }
    MixedLeg trial[3] = {legs[0], legs[1], legs[2]};
    trial[channel.leg].cents += (int64_t)channel.amount * 100;
    const Quote quote = quoteMixedSats(trial, 3);
    if (quote.ok && quote.sats <= walletSats) {
      mask |= (uint16_t)(1u << i);
    }
  }
  return mask;
}
