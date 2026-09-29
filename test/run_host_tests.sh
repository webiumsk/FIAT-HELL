#!/bin/sh
# Host checks for the payout, portal, and OTA rules. No device required.
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
CXX=${CXX:-c++}
KEY=${OTA_SIGNING_KEY:-tools/ota_private.pem}
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT

echo "== payout, portal, version =="
$CXX -std=c++17 -Wall -Wextra -Werror -Isrc -o "$BUILD/payout_test" \
  test/payout_test.cpp \
  src/payout/Quote.cpp \
  src/payout/Bolt11.cpp \
  src/payout/NoteMask.cpp \
  src/payout/OfflineLnurl.cpp
"$BUILD/payout_test"

if [ ! -f "$KEY" ]; then
  echo "== OTA signature checks skipped: $KEY not present =="
  echo "host tests ok"
  exit 0
fi

echo "== public key matches the private key =="
python3 - "$KEY" <<'PY'
import pathlib, re, subprocess, sys
der = subprocess.check_output(
    ["openssl", "pkey", "-in", sys.argv[1], "-pubout", "-outform", "DER"]
)
header = pathlib.Path("src/ota/ota_pubkey.h").read_text()
nums = [int(n) for n in re.findall(r"\d+", header.split("{", 1)[1].split("}", 1)[0])]
if bytes(nums) != der[-32:]:
    sys.exit("src/ota/ota_pubkey.h does not match the signing key")
print("pubkey matches")
PY

echo "== sign_firmware.py output verifies in the device verifier =="
# TweetNaCl needs a 32-bit u32; `unsigned long` is 32-bit on the ESP32 but
# 64-bit here, so the host copy is retyped.
sed 's/typedef unsigned long u32;/typedef unsigned int u32;/' \
  src/ota/tweetnacl.c > "$BUILD/tweetnacl_host.c"
$CC -std=c99 -Isrc/ota -c "$BUILD/tweetnacl_host.c" -o "$BUILD/tweetnacl.o"
$CXX -std=c++17 -Wall -Wextra -Werror -Itest/host -Isrc -o "$BUILD/ota_verify" \
  test/ota_verify_host.cpp src/ota/OtaImage.cpp "$BUILD/tweetnacl.o"

IMAGE="$BUILD/fiat-hell-v9.9.9.bin"
dd if=/dev/urandom of="$IMAGE" bs=4096 count=8 >/dev/null 2>&1
printf 'FHFW:9.9.9' >> "$IMAGE"
OTA_SIGNING_KEY="$KEY" python3 tools/sign_firmware.py "$IMAGE"
"$BUILD/ota_verify" "$IMAGE" "$IMAGE.sig"
echo "signed image accepted"

python3 - "$IMAGE" <<'PY'
import pathlib, sys
image = pathlib.Path(sys.argv[1])
data = bytearray(image.read_bytes())
data[0] ^= 0x01
image.write_bytes(data)
PY
if "$BUILD/ota_verify" "$IMAGE" "$IMAGE.sig"; then
  echo "tampered image was accepted" >&2
  exit 1
fi
echo "tampered image rejected"

openssl genpkey -algorithm ED25519 -out "$BUILD/other.pem"
openssl pkeyutl -sign -inkey "$BUILD/other.pem" -rawin \
  -in "$IMAGE" -out "$BUILD/other.sig"
if "$BUILD/ota_verify" "$IMAGE" "$BUILD/other.sig"; then
  echo "signature from another key was accepted" >&2
  exit 1
fi
echo "other key rejected"

UNMARKED="$BUILD/unmarked.bin"
dd if=/dev/urandom of="$UNMARKED" bs=4096 count=1 >/dev/null 2>&1
if OTA_SIGNING_KEY="$KEY" python3 tools/sign_firmware.py "$UNMARKED" 2>/dev/null; then
  echo "sign_firmware.py signed an image without a version marker" >&2
  exit 1
fi
echo "unmarked image refused by sign_firmware.py"

echo "host tests ok"
