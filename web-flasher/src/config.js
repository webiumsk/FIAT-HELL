export const FW_VERSION = 'FW_VERSION_PLACEHOLDER';

// GitHub repo for fetching tagged releases.
export const GITHUB_REPO = 'webiumsk/FIAT-HELL';

// Supported hardware targets. NOTE the differing bootloader offset:
// ESP32-S3 boots from 0x0, classic ESP32 (WT32-SC01) from 0x1000.
export const BOARDS = {
  s3: {
    label: 'ESP32-8048S050 (S3)',
    assetSuffix: '-s3',
    manualBoot: true, // hold BOOT + RESET before flashing
    offsets: {
      'bootloader.bin': 0x00000,
      'partitions.bin': 0x08000,
      'boot_app0.bin':  0x0E000,
      'firmware.bin':   0x10000,
    },
    // Local "latest from this page deploy" — relative URLs to bundled bins.
    localParts: [
      { path: 'bin/s3/bootloader.bin', offset: 0x00000 },
      { path: 'bin/s3/partitions.bin', offset: 0x08000 },
      { path: 'bin/s3/boot_app0.bin',  offset: 0x0E000 },
      { path: 'bin/s3/firmware.bin',   offset: 0x10000 },
    ],
  },
  wt32: {
    label: 'WT32-SC01 (ESP32)',
    assetSuffix: '-wt32',
    manualBoot: false, // auto-reset via DTR/RTS usually works
    offsets: {
      'bootloader.bin': 0x01000,
      'partitions.bin': 0x08000,
      'boot_app0.bin':  0x0E000,
      'firmware.bin':   0x10000,
    },
    localParts: [
      { path: 'bin/wt32/bootloader.bin', offset: 0x01000 },
      { path: 'bin/wt32/partitions.bin', offset: 0x08000 },
      { path: 'bin/wt32/boot_app0.bin',  offset: 0x0E000 },
      { path: 'bin/wt32/firmware.bin',   offset: 0x10000 },
    ],
  },
};

export const DEFAULT_BOARD = 's3';
