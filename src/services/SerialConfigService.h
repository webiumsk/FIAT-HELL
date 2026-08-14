#pragma once

#include <FS.h>

/**
 * Serial provisioning window used by the web flasher right after boot.
 *
 * Protocol (device lines are prefixed FIAT-HELL:):
 *   device: FIAT-HELL:CONFIG_READY
 *   host:   SCAN_WIFI                          (optional)
 *   device: FIAT-HELL:WIFI_LIST:[{"ssid":...,"rssi":...,"secure":...},...]
 *   host:   READ_CONFIG                        (optional)
 *   device: FIAT-HELL:CONFIG_FILE:/file.json:{json}   (per stored file;
 *           secrets are replaced by "__SET__" and never leave the device)
 *   device: FIAT-HELL:CONFIG_DUMP_DONE
 *   host:   WRITE_CONFIG:/file.json:{json}     (repeated; a "__KEEP__"
 *           value keeps the currently stored secret)
 *   device: FIAT-HELL:WROTE:/file.json
 *   host:   CONFIG_DONE
 *   device: FIAT-HELL:CONFIG_SAVED
 *
 * Blocks only when data arrives within the 2 s detection window, so normal
 * boots are not delayed. Call after the filesystem is mounted and before
 * config files are read.
 */
namespace SerialConfigService {

void runSerialConfigWindow(fs::FS &fs);

} // namespace SerialConfigService
