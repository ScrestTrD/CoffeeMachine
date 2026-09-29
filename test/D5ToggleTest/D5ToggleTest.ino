/*
  D5ToggleTest — Test chan D7 (NodeMCU D7 = GPIO13).
  Hanh vi: D7 ON 20s -> OFF 20s, lap lai. Mo Serial @115200 de xem trang thai.

  Chu y: neu mach van dang dau, relay se dong/ngat moi giay.
*/

#include <Arduino.h>

#define TEST_PIN D7
#define TOGGLE_MS 20000

void setup() {
  pinMode(TEST_PIN, OUTPUT);
  digitalWrite(TEST_PIN, LOW);
  Serial.begin(115200);
  delay(200);
  Serial.println("D5ToggleTest: D7 ON 20s / OFF 20s");
}

void loop() {
  digitalWrite(TEST_PIN, HIGH);
  Serial.println("D7 = HIGH (ON)");
  delay(TOGGLE_MS);
  digitalWrite(TEST_PIN, LOW);
  Serial.println("D7 = LOW (OFF)");
  delay(TOGGLE_MS);
}
