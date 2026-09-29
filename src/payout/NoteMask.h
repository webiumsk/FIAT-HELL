#pragma once

#include "payout/Quote.h"

#include <stdint.h>

// Which bill channels may stay open so the wallet can still pay the sale.
// No Arduino types, so host tests can compile it.

enum class BalanceUnit { Sats, Msats, UsdCents };

// The wallet balance as whole sats. Msats (LNbits) are rounded down. UsdCents
// (Stablesats, Flash) need the BTC price in USD. Fails on an empty balance or
// a missing price, so a wallet that cannot be priced never opens the acceptor.
bool walletBalanceSats(BalanceUnit unit, int64_t balance, double btcUsdPrice,
                       int64_t *satsOut);

struct NoteChannel {
  int leg;    // 0..2, the configured currency this channel belongs to
  int amount; // whole currency units
};

// Bit i is set when adding one note of channel i to the legs already inserted
// still quotes to a positive number of sats the wallet can pay. A channel
// whose currency has no usable price or fee stays closed. At most 16 channels.
uint16_t fittingNoteMask(const NoteChannel *channels, int count,
                         const MixedLeg legs[3], int64_t walletSats);
