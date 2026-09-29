// Runs the device verifier (src/ota/OtaImage.cpp) over an image and a
// signature file written by tools/sign_firmware.py. Exit 0 = valid.

#include "ota/OtaImage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *readFile(const char *path, size_t offset, size_t *len) {
  FILE *file = fopen(path, "rb");
  if (file == nullptr) {
    return nullptr;
  }
  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  rewind(file);
  unsigned char *buffer =
      size >= 0 ? (unsigned char *)malloc((size_t)size + offset) : nullptr;
  if (buffer == nullptr ||
      fread(buffer + offset, 1, (size_t)size, file) != (size_t)size) {
    free(buffer);
    fclose(file);
    return nullptr;
  }
  fclose(file);
  *len = (size_t)size;
  return buffer;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: ota_verify_host <image> <sig>\n");
    return 2;
  }
  size_t imageLen = 0;
  size_t sigLen = 0;
  // Same layout the OTA handler builds: signature(64) || image.
  unsigned char *signedImage = readFile(argv[1], 64, &imageLen);
  unsigned char *signature = readFile(argv[2], 0, &sigLen);
  if (signedImage == nullptr || signature == nullptr || sigLen != 64) {
    fprintf(stderr, "could not read image and 64-byte signature\n");
    free(signedImage);
    free(signature);
    return 2;
  }
  memcpy(signedImage, signature, 64);
  const bool ok = otaSignatureOk(signedImage, imageLen + 64);
  free(signedImage);
  free(signature);
  return ok ? 0 : 1;
}
