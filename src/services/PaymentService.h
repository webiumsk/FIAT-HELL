#pragma once

#include <Arduino.h>
#include <cstring>

class PaymentService {
public:
  bool isBlink(const char *fundingSource) const;
  bool isFlash(const char *fundingSource) const;
  // Blink and Flash both run the Galoy GraphQL API (Flash is a Galoy fork)
  bool isGaloy(const char *fundingSource) const;
  bool hasLNbitsConfig(const char *url, const char *adminkey,
                       const char *readkey) const;
};

