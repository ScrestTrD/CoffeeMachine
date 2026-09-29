/*
  ButtonTest — Doc trang thai 2 nut cua may ca phe theo thoi gian thuc.
  SET: D0 (GPIO16), active-LOW (nhan = LOW), pull-up ngoai 10k.
  RUN: D8 (GPIO15), active-HIGH (nhan = HIGH), pull-down ngoai 10k.

  In Serial @115200 moi 200ms + ngay khi doi trang thai.

  Chu y:
  - De NGUYEN INPUT, khong dung INPUT_PULLUP (D0 khong co pull-up noi chuan;
    pull-up D8 pha boot strapping + choi voi pull-down ngoai).
  - Tren board tran khong co dien tro ngoai thi chan noi lung bung (floating):
    muon doc dung phai gan pull-up 10k cho D0 (len 3.3V) va pull-down 10k
    cho D8 (xuong GND) nhu tren may that.
*/

#include <Arduino.h>

#define PIN_SETB D0
#define PIN_RUNB D8
const uint32_t PRINT_PERIOD_MS = 200;

bool lastSetPressed = false;
bool lastRunPressed = false;
uint32_t lastPrintMs = 0;
bool firstRun = true;

void setup() {
  pinMode(PIN_SETB, INPUT);   // giu nguyen, phu thuoc tro ngoai
  pinMode(PIN_RUNB, INPUT);
  Serial.begin(115200);
  delay(200);
  Serial.println("ButtonTest: SET=D0 active-LOW, RUN=D8 active-HIGH");
  Serial.println("SET_raw | RUN_raw | SET_pressed | RUN_pressed");
}

void loop() {
  uint32_t now = millis();
  bool setRaw = digitalRead(PIN_SETB) ? true : false;
  bool runRaw = digitalRead(PIN_RUNB) ? true : false;
  bool setPressed = (setRaw == LOW);    // active-LOW
  bool runPressed = (runRaw == HIGH);   // active-HIGH

  bool changed = firstRun || (setPressed != lastSetPressed) || (runPressed != lastRunPressed);
  if (changed || (now - lastPrintMs) >= PRINT_PERIOD_MS) {
    lastPrintMs = now;
    firstRun = false;
    lastSetPressed = setPressed;
    lastRunPressed = runPressed;
    Serial.print(setRaw ? "HIGH" : "LOW ");
    Serial.print(" | ");
    Serial.print(runRaw ? "HIGH" : "LOW ");
    Serial.print(" | ");
    Serial.print(setPressed ? "SET_NHAN " : "SET_nha   ");
    Serial.print(" | ");
    Serial.println(runPressed ? "RUN_NHAN" : "RUN_nha ");
  }
}
