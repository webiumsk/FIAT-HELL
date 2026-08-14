#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("MINIMAL BOOT OK");
}

void loop() {
  Serial.printf("tick %lu\n", millis());
  delay(1000);
}
