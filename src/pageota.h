#ifndef PAGE_OTA_H
#define PAGE_OTA_H

#include <Arduino.h>

static const char PAGE_OTA[] PROGMEM = R"(
{
  "uri": "/ota",
  "title": "Update",
  "menu": true,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "<h4>Firmware update</h4><p>Select version and update from HTTPS server</p>",
      "style": ""
    },
    {
      "name": "version",
      "type": "ACSelect",
      "label": "Firmware version",
      "option": []
    },
    {
      "name": "update",
      "type": "ACSubmit",
      "value": "Update",
      "uri": "/ota_do"
    }
  ]
}
)";

static const char PAGE_OTA_DO[] PROGMEM = R"(
{
  "uri": "/ota_do",
  "title": "Update",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "<h4>Firmware update</h4>",
      "style": ""
    },
    {
      "name": "result",
      "type": "ACText",
      "value": "Processing...",
      "style": "font-size: 18px; color: #333; min-height: 2em;"
    }
  ]
}
)";

#endif
