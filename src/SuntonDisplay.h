#pragma once

#include <Arduino.h>
#include <TAMC_GT911.h>
#include <Wire.h>
#include <Arduino_DataBus.h>
#include <databus/Arduino_ESP32RGBPanel.h>
#include <display/Arduino_RGB_Display.h>

class SuntonDisplay {
public:
  SuntonDisplay()
      : _bus(DE, VSYNC, HSYNC, PCLK, DATA_R0, DATA_R1, DATA_R2, DATA_R3,
             DATA_R4, DATA_G0, DATA_G1, DATA_G2, DATA_G3, DATA_G4, DATA_G5,
             DATA_B0, DATA_B1, DATA_B2, DATA_B3, DATA_B4, HSYNC_POLARITY,
             HSYNC_FRONT_PORCH, HSYNC_PULSE_WIDTH, HSYNC_BACK_PORCH,
             VSYNC_POLARITY, VSYNC_FRONT_PORCH, VSYNC_PULSE_WIDTH,
             VSYNC_BACK_PORCH, PCLK_ACTIVE_NEG, PREFER_SPEED, false, 0, 0),
        _gfx(PANEL_WIDTH, PANEL_HEIGHT, &_bus, 0, AUTO_FLUSH),
        _touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST,
               max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2)) {}

  bool init() {
    if (!_gfx.begin()) {
      return false;
    }

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000);
    const uint8_t primaryTouchAddress = probeTouchAddress();
    const uint8_t fallbackTouchAddress =
        primaryTouchAddress == GT911_ADDR1 ? GT911_ADDR2 : GT911_ADDR1;
    _touch.begin(primaryTouchAddress);
    if (!probeTouchController(primaryTouchAddress)) {
      _touch.begin(fallbackTouchAddress);
      if (!probeTouchController(fallbackTouchAddress)) {
        _touchReady = false;
        _gfx.fillScreen(0x0000);
        return true;
      }
    }
    _touch.setRotation(ROTATION_NORMAL);
    _touchReady = true;
    _gfx.fillScreen(0x0000);
    return true;
  }

  int16_t width() const { return _gfx.width(); }

  int16_t height() const { return _gfx.height(); }

  void setRotation(uint8_t rotation) {
    _rotation = rotation & 3U;
    _gfx.setRotation(_rotation);
  }

  uint8_t getRotation() const { return _rotation; }

  void drawBitmap565(int16_t x, int16_t y, uint16_t *bitmap, int16_t w,
                     int16_t h) {
    _gfx.draw16bitRGBBitmap(x, y, bitmap, w, h);
  }

  void fillScreen(uint16_t color) { _gfx.fillScreen(color); }

  bool getTouch(uint16_t *x, uint16_t *y) {
    if (!_touchReady) {
      return false;
    }

    _touch.read();
    if (!_touch.isTouched) {
      return false;
    }

    int16_t tx =
        map(_touch.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, width() - 1);
    int16_t ty =
        map(_touch.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, height() - 1);

    tx = constrain(tx, 0, width() - 1);
    ty = constrain(ty, 0, height() - 1);

    *x = static_cast<uint16_t>(tx);
    *y = static_cast<uint16_t>(ty);
    return true;
  }

private:
  uint8_t probeTouchAddress() {
    return probeTouchController(GT911_ADDR1) ? GT911_ADDR1 : GT911_ADDR2;
  }

  bool probeTouchController(uint8_t address) {
    Wire.beginTransmission(address);
    Wire.write(highByte(GT911_PRODUCT_ID));
    Wire.write(lowByte(GT911_PRODUCT_ID));
    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    return Wire.requestFrom(address, static_cast<uint8_t>(1)) == 1;
  }

  static constexpr int16_t PANEL_WIDTH = 800;
  static constexpr int16_t PANEL_HEIGHT = 480;

  static constexpr bool AUTO_FLUSH = true;
  static constexpr bool PCLK_ACTIVE_NEG = true;
  static constexpr uint32_t PREFER_SPEED = 16000000;
  static constexpr uint8_t HSYNC_POLARITY = 0;
  static constexpr uint8_t HSYNC_FRONT_PORCH = 8;
  static constexpr uint8_t HSYNC_BACK_PORCH = 8;
  static constexpr uint8_t HSYNC_PULSE_WIDTH = 4;
  static constexpr uint8_t VSYNC_POLARITY = 0;
  static constexpr uint8_t VSYNC_FRONT_PORCH = 8;
  static constexpr uint8_t VSYNC_BACK_PORCH = 8;
  static constexpr uint8_t VSYNC_PULSE_WIDTH = 4;

  static constexpr int8_t TFT_BL = 2;
  static constexpr int8_t DE = 40;
  static constexpr int8_t VSYNC = 41;
  static constexpr int8_t HSYNC = 39;
  static constexpr int8_t PCLK = 42;

  // These GPIO mappings follow the working ST7262 esp_lcd configuration for
  // the ESP32-8048S050C board.
  static constexpr int DATA_R0 = 45;
  static constexpr int DATA_R1 = 48;
  static constexpr int DATA_R2 = 47;
  static constexpr int DATA_R3 = 21;
  static constexpr int DATA_R4 = 14;
  static constexpr int DATA_G0 = 5;
  static constexpr int DATA_G1 = 6;
  static constexpr int DATA_G2 = 7;
  static constexpr int DATA_G3 = 15;
  static constexpr int DATA_G4 = 16;
  static constexpr int DATA_G5 = 4;
  static constexpr int DATA_B0 = 8;
  static constexpr int DATA_B1 = 3;
  static constexpr int DATA_B2 = 46;
  static constexpr int DATA_B3 = 9;
  static constexpr int DATA_B4 = 1;

  // 800x480: touch X inverted; Y inverted (reported touch was above actual tap)
  static constexpr int16_t TOUCH_MAP_X1 = 800;
  static constexpr int16_t TOUCH_MAP_X2 = 0;
  static constexpr int16_t TOUCH_MAP_Y1 = 480;
  static constexpr int16_t TOUCH_MAP_Y2 = 0;

  static constexpr uint8_t TOUCH_SDA = 19;
  static constexpr uint8_t TOUCH_SCL = 20;
  static constexpr int8_t TOUCH_INT = -1;
  static constexpr uint8_t TOUCH_RST = 38;

  Arduino_ESP32RGBPanel _bus;
  Arduino_RGB_Display _gfx;
  TAMC_GT911 _touch;
  uint8_t _rotation = 0;
  bool _touchReady = false;
};
