export const FW_VERSION = 'FW_VERSION_PLACEHOLDER';

// Local "latest from this page deploy" — relative URLs to bundled bins.
export const FLASH_PARTS = [
  { path: 'bin/bootloader.bin', offset: 0x00000 },
  { path: 'bin/partitions.bin', offset: 0x08000 },
  { path: 'bin/boot_app0.bin',  offset: 0x0E000 },
  { path: 'bin/firmware.bin',   offset: 0x10000 },
];

// GitHub repo for fetching tagged releases.
export const GITHUB_REPO = 'webiumsk/FIAT-HELL';

// Flash address layout (used to map release asset names to offsets).
export const FLASH_OFFSETS = {
  'bootloader.bin': 0x00000,
  'partitions.bin': 0x08000,
  'boot_app0.bin':  0x0E000,
  'firmware.bin':   0x10000,
};
