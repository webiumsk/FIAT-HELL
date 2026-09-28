#include "payout/OfflineLnurl.h"

#include <math.h>

OfflinePayout offlineLnurlPayout(FundingKind kind, bool mixed, int64_t cents,
                                 double feePercent) {
  OfflinePayout payout;
  payout.allowed = false;
  payout.centsAfterFee = 0;
  if (kind != FundingKind::Lnbits || mixed) {
    return payout;
  }
  if (cents <= 0 || !(feePercent == feePercent) || feePercent < 0.0 ||
      feePercent > 100.0) {
    return payout;
  }
  const double afterFee =
      static_cast<double>(cents) * (100.0 - feePercent) / 100.0;
  const long long rounded = llround(afterFee);
  if (rounded <= 0) {
    return payout;
  }
  payout.allowed = true;
  payout.centsAfterFee = (uint64_t)rounded;
  return payout;
}
