#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// True when fileMajor.fileMinor.filePatch is strictly older than the
// "X.Y.Z" the device runs. An unparseable current version rejects everything.
inline bool firmwareVersionTupleOlder(int fileMajor, int fileMinor,
                                      int filePatch, const char *current) {
  int curMajor = 0, curMinor = 0, curPatch = 0;
  if (current == nullptr ||
      sscanf(current, "%d.%d.%d", &curMajor, &curMinor, &curPatch) != 3) {
    return true;
  }
  if (fileMajor != curMajor) {
    return fileMajor < curMajor;
  }
  if (fileMinor != curMinor) {
    return fileMinor < curMinor;
  }
  return filePatch < curPatch;
}

// True when the selected firmware name must not be installed: it has no
// vMAJOR.MINOR.PATCH, or that version is older than the running firmware.
// An equal version is allowed so a device can reinstall the build it runs.
inline bool firmwareUpdateRejected(const char *filename, const char *current) {
  if (filename == nullptr || current == nullptr) {
    return true;
  }
  const char *mark = strrchr(filename, 'v');
  if (mark == nullptr) {
    mark = strrchr(filename, 'V');
  }
  int fileMajor = 0, fileMinor = 0, filePatch = 0;
  if (mark == nullptr ||
      (sscanf(mark, "v%d.%d.%d", &fileMajor, &fileMinor, &filePatch) != 3 &&
       sscanf(mark, "V%d.%d.%d", &fileMajor, &fileMinor, &filePatch) != 3)) {
    return true;
  }
  return firmwareVersionTupleOlder(fileMajor, fileMinor, filePatch, current);
}

// The firmware embeds "FHFW:X.Y.Z" (kFirmwareVersionMarker in main.cpp), so
// the downgrade check works on the image itself and a renamed file cannot
// fake a newer version. True when no marker carries a full version or that
// version is older than the running firmware. The bare "FHFW:" needle below
// is itself linked into the image, so an occurrence without a version is
// skipped rather than treated as the marker.
inline bool firmwareImageDowngrade(const uint8_t *image, size_t imageLen,
                                   const char *current) {
  if (image == nullptr || current == nullptr) {
    return true;
  }
  static const char marker[] = "FHFW:";
  const size_t markerLen = sizeof(marker) - 1;
  for (size_t i = 0; i + markerLen < imageLen; i++) {
    if (memcmp(image + i, marker, markerLen) != 0) {
      continue;
    }
    char version[16];
    size_t versionLen = 0;
    size_t j = i + markerLen;
    while (j < imageLen && versionLen < sizeof(version) - 1 &&
           ((image[j] >= '0' && image[j] <= '9') || image[j] == '.')) {
      version[versionLen++] = (char)image[j++];
    }
    version[versionLen] = '\0';
    int fileMajor = 0, fileMinor = 0, filePatch = 0;
    if (sscanf(version, "%d.%d.%d", &fileMajor, &fileMinor, &filePatch) == 3) {
      return firmwareVersionTupleOlder(fileMajor, fileMinor, filePatch,
                                       current);
    }
  }
  return true;
}
