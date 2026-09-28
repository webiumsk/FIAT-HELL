#pragma once
#include <stdint.h>
// Raw Ed25519 public key. The matching private key is tools/ota_private.pem
// and is not committed. tools/sign_firmware.py produces a 64-byte signature.
static const uint8_t OTA_PUBKEY[32] = { 35, 86, 50, 202, 221, 216, 129, 241, 177, 114, 157, 91, 88, 217, 93, 213, 232, 111, 131, 31, 189, 105, 103, 92, 150, 121, 18, 242, 189, 31, 150, 75 };
