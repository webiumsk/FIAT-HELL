export const FW_VERSION = 'FW_VERSION_PLACEHOLDER';

export const FLASH_PARTS = [
  { path: 'bin/bootloader.bin', offset: 0x00000 },
  { path: 'bin/partitions.bin', offset: 0x08000 },
  { path: 'bin/boot_app0.bin',  offset: 0x0E000 },
  { path: 'bin/firmware.bin',   offset: 0x10000 },
];
