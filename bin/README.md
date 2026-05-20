# Firmware binárky

Tento priečinok musí obsahovať tieto súbory pred nasadením na GitHub Pages:

| Súbor | Zdroj | Popis |
|-------|-------|-------|
| `bootloader.bin` | `.pio/build/esp32-8048s050/bootloader.bin` | ESP32-S3 bootloader |
| `partitions.bin` | `.pio/build/esp32-8048s050/partitions.bin` | Partition table |
| `boot_app0.bin` | `.pio/build/esp32-8048s050/boot_app0.bin` | OTA support (alebo z `~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin`) |
| `firmware.bin` | `.pio/build/esp32-8048s050/firmware.bin` | Hlavný firmware |

## Postup

```bash
# 1. Zbuildni projekt
pio run

# 2. Skopíruj binárky
cp .pio/build/esp32-8048s050/bootloader.bin web-flasher/bin/
cp .pio/build/esp32-8048s050/partitions.bin web-flasher/bin/
cp .pio/build/esp32-8048s050/firmware.bin   web-flasher/bin/

# boot_app0.bin – nájdi cestu cez PlatformIO
find ~/.platformio -name "boot_app0.bin" | head -1
cp <cesta>/boot_app0.bin web-flasher/bin/
```

## Aktualizácia verzie vo flasheri

Po skopírovaní nových binárok aktualizuj aj `version` v `manifest.json`.

> **Poznámka:** Súbory `*.bin` sú v `.gitignore`. Pri CI/CD pipeline ich treba generovať v GitHub Actions
> a commitnúť do `gh-pages` branch alebo nahrať ako GitHub Release assets.
