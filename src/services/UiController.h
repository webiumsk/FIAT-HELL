#pragma once

#include <lvgl.h>

class UiController {
public:
  UiController(lv_obj_t *&logo, lv_obj_t *&portal, lv_obj_t *&api,
               lv_obj_t *&thx, lv_obj_t *&main, lv_obj_t *&insert,
               lv_obj_t *&qr, lv_obj_t *&currency)
      : screen_logo(logo), screen_portal(portal), screen_api(api),
        screen_thx(thx), screen_main(main), screen_insert_money(insert),
        screen_qr(qr), screen_currency(currency) {}

  void deleteMainScreen();
  void deleteLogoScreen();
  void deleteCurrencyScreen();
  void deleteInsertMoneyScreen();
  void deleteQRCodeScreen();
  void deleteThankYouScreen();
  void deleteAllScreens();

private:
  lv_obj_t *&screen_logo;
  lv_obj_t *&screen_portal;
  lv_obj_t *&screen_api;
  lv_obj_t *&screen_thx;
  lv_obj_t *&screen_main;
  lv_obj_t *&screen_insert_money;
  lv_obj_t *&screen_qr;
  lv_obj_t *&screen_currency;
};

