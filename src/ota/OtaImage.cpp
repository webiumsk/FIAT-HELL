#include "ota/OtaImage.h"
#include "ota/ota_pubkey.h"
#include "ota/tweetnacl.h"

#include <esp_heap_caps.h>
#include <stdlib.h>

bool otaSignatureOk(const uint8_t *signedImage, size_t signedLen) {
  if (signedImage == nullptr || signedLen <= 64) {
    return false;
  }
  uint8_t *opened = (uint8_t *)heap_caps_malloc(signedLen, MALLOC_CAP_SPIRAM);
  if (opened == nullptr) {
    return false;
  }
  unsigned long long messageLen = 0;
  const int verified =
      crypto_sign_open(opened, &messageLen, signedImage, signedLen, OTA_PUBKEY);
  free(opened);
  return verified == 0 && messageLen == signedLen - 64;
}
