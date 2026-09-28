// Stand-in for the ESP-IDF header so src/ota/OtaImage.cpp builds on the host.
#pragma once

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM 0

static inline void *heap_caps_malloc(size_t size, unsigned caps) {
  (void)caps;
  return malloc(size);
}
