#!/usr/bin/env python3
"""Write a 64-byte Ed25519 signature next to a firmware image.

The private key defaults to tools/ota_private.pem (gitignored); set
OTA_SIGNING_KEY or pass --key to keep it outside the repository. Generate it
once with:
  openssl genpkey -algorithm ED25519 -out tools/ota_private.pem
The matching public key is compiled into src/ota/ota_pubkey.h.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_KEY = ROOT / "tools" / "ota_private.pem"
# Same marker the device looks for (kFirmwareVersionMarker in main.cpp). An
# image without it is refused by the device after a full download.
MARKER = re.compile(rb"FHFW:(\d+)\.(\d+)\.(\d+)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument(
        "--key",
        type=Path,
        default=Path(os.environ.get("OTA_SIGNING_KEY", DEFAULT_KEY)),
    )
    args = parser.parse_args()
    firmware = args.firmware
    if not firmware.is_file():
        sys.exit(f"firmware not found: {firmware}")
    if not args.key.is_file():
        sys.exit(f"private key not found: {args.key}")
    marker = MARKER.search(firmware.read_bytes())
    if marker is None:
        sys.exit(f"{firmware} has no FHFW:X.Y.Z version marker; not an S3 build")
    signature = firmware.with_name(firmware.name + ".sig")
    subprocess.check_call(
        [
            "openssl",
            "pkeyutl",
            "-sign",
            "-inkey",
            str(args.key),
            "-rawin",
            "-in",
            str(firmware),
            "-out",
            str(signature),
        ]
    )
    size = signature.stat().st_size
    if size != 64:
        sys.exit(f"signature is {size} bytes, expected 64")
    version = b".".join(marker.groups()).decode()
    print(f"wrote {signature} (firmware {version})")


if __name__ == "__main__":
    main()
