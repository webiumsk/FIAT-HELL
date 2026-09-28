#pragma once

#include <stddef.h>
#include <stdint.h>

// Ed25519 over a buffer laid out as signature(64) || image. This is the exact
// input TweetNaCl's crypto_sign_open takes, so the download can land the image
// directly behind the signature with no extra copy. One PSRAM scratch buffer
// of signedLen is allocated for the opened message; in-place verification is
// not possible (crypto_sign_open overwrites S with the public key before
// reading it). This core's mbedTLS build has no Ed25519.
bool otaSignatureOk(const uint8_t *signedImage, size_t signedLen);
