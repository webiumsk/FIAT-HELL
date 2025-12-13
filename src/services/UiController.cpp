#include "services/UiController.h"
#include <Arduino.h>

void UiController::deleteMainScreen() {
  if (screen_main != NULL) {
    lv_obj_del(screen_main);
    screen_main = NULL;
    Serial.println(F("Delete: screen_main"));
  }
}

void UiController::deleteLogoScreen() {
  if (screen_logo != NULL) {
    lv_obj_del(screen_logo);
    screen_logo = NULL;
    Serial.println(F("Delete: screen_logo"));
  }
}

void UiController::deleteCurrencyScreen() {
  if (screen_currency != NULL) {
    lv_obj_del(screen_currency);
    screen_currency = NULL;
    Serial.println(F("Delete: screen_currency"));
  }
}

void UiController::deleteInsertMoneyScreen() {
  if (screen_insert_money != NULL) {
    lv_obj_del(screen_insert_money);
    screen_insert_money = NULL;
    Serial.println(F("Delete: screen_insert_money"));
  }
}

void UiController::deleteQRCodeScreen() {
  if (screen_qr != NULL) {
    lv_obj_del(screen_qr);
    screen_qr = NULL;
    Serial.println(F("Delete: screen_qr"));
  }
}

void UiController::deleteThankYouScreen() {
  if (screen_thx != NULL) {
    lv_obj_del(screen_thx);
    screen_thx = NULL;
    Serial.println(F("Delete: screen_thx"));
  }
}

void UiController::deleteAllScreens() {
  deleteLogoScreen();
  deleteCurrencyScreen();
  deleteInsertMoneyScreen();
  deleteQRCodeScreen();
  deleteThankYouScreen();
  deleteMainScreen();
}

