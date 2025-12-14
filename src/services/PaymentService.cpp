#include "services/PaymentService.h"

bool PaymentService::isBlink(const char *fundingSource) const {
  return fundingSource && strcmp(fundingSource, "Blink") == 0;
}

bool PaymentService::hasLNbitsConfig(const char *url, const char *adminkey,
                                     const char *readkey) const {
  return url && adminkey && readkey && url[0] != '\0' && adminkey[0] != '\0' &&
         readkey[0] != '\0';
}

