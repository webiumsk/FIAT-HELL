#pragma once

#include <Arduino.h>
#include <cstring>

class PaymentService {
public:
  bool isBlink(const char *fundingSource) const;
  bool hasLNbitsConfig(const char *url, const char *adminkey,
                       const char *readkey) const;
};

