#include "services/PaymentService.h"
#include "services/FundingService.h"

bool PaymentService::isBlink(const char *fundingSource) const {
  return fundingSource && strcmp(fundingSource, "Blink") == 0;
}

bool PaymentService::isFlash(const char *fundingSource) const {
  return fundingSource && strcmp(fundingSource, "Flash") == 0;
}

bool PaymentService::isGaloy(const char *fundingSource) const {
  // Single source of truth for which providers speak the Galoy API.
  return FundingService::isGaloy(fundingSource);
}

bool PaymentService::hasLNbitsConfig(const char *url, const char *adminkey,
                                     const char *readkey) const {
  return url && adminkey && readkey && url[0] != '\0' && adminkey[0] != '\0' &&
         readkey[0] != '\0';
}

