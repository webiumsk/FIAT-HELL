#include "payout/Bolt11.h"

#include <ctype.h>
#include <string.h>

static char lowerChar(char c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return c;
}

static const char *skipLightningPrefix(const char *invoice) {
  while (*invoice == ' ' || *invoice == '\t' || *invoice == '\r' ||
         *invoice == '\n') {
    invoice++;
  }
  const char prefix[] = "lightning:";
  size_t i = 0;
  for (; prefix[i] != '\0'; i++) {
    if (lowerChar(invoice[i]) != prefix[i]) {
      return invoice;
    }
  }
  invoice += i;
  while (*invoice == ' ' || *invoice == '\t') {
    invoice++;
  }
  return invoice;
}

const char *bolt11PaymentRequest(const char *invoice) {
  if (invoice == nullptr) {
    return nullptr;
  }
  return skipLightningPrefix(invoice);
}

bool bolt11AmountSats(const char *invoice, int64_t *satsOut) {
  if (satsOut != nullptr) {
    *satsOut = 0;
  }
  if (invoice == nullptr || satsOut == nullptr) {
    return false;
  }
  invoice = bolt11PaymentRequest(invoice);

  const char *separator = strrchr(invoice, '1');
  if (separator == nullptr || separator == invoice) {
    return false;
  }

  char hrp[32];
  const size_t hrpLen = (size_t)(separator - invoice);
  if (hrpLen == 0 || hrpLen >= sizeof(hrp)) {
    return false;
  }
  for (size_t i = 0; i < hrpLen; i++) {
    hrp[i] = lowerChar(invoice[i]);
  }
  hrp[hrpLen] = '\0';

  const char *amount = nullptr;
  if (strncmp(hrp, "lnbcrt", 6) == 0) {
    amount = hrp + 6;
  } else if (strncmp(hrp, "lnbc", 4) == 0) {
    amount = hrp + 4;
  } else if (strncmp(hrp, "lntb", 4) == 0) {
    amount = hrp + 4;
  } else {
    return false;
  }
  if (amount[0] == '\0') {
    return false;
  }

  // Positive decimal integer, no leading zeros, then an optional multiplier.
  if (amount[0] < '1' || amount[0] > '9') {
    return false;
  }
  uint64_t number = 0;
  const char *p = amount;
  while (*p >= '0' && *p <= '9') {
    const uint64_t digit = (uint64_t)(*p - '0');
    if (number > (UINT64_MAX - digit) / 10) {
      return false;
    }
    number = number * 10 + digit;
    p++;
  }

  // Sats per 1 unit of the HRP number.
  // none: BTC, m: milli, u: micro, n: nano (1/10 sat), p: pico (1/10000 sat).
  uint64_t sats = 0;
  if (*p == '\0') {
    if (number > UINT64_MAX / 100000000ULL) {
      return false;
    }
    sats = number * 100000000ULL;
  } else if (p[1] != '\0') {
    return false;
  } else if (*p == 'm') {
    if (number > UINT64_MAX / 100000ULL) {
      return false;
    }
    sats = number * 100000ULL;
  } else if (*p == 'u') {
    if (number > UINT64_MAX / 100ULL) {
      return false;
    }
    sats = number * 100ULL;
  } else if (*p == 'n') {
    if (number % 10ULL != 0) {
      return false;
    }
    sats = number / 10ULL;
  } else if (*p == 'p') {
    if (number % 10000ULL != 0) {
      return false;
    }
    sats = number / 10000ULL;
  } else {
    return false;
  }

  if (sats == 0 || sats > (uint64_t)INT64_MAX) {
    return false;
  }
  *satsOut = (int64_t)sats;
  return true;
}
