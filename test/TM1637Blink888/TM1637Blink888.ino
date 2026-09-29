/*
  TM1637Blink888 — nhap nhay 888 vo han tren module LED 3 so (IC TM1637).
  Thu vien: TM1637 cua Avishorp. CLK = D3, DIO = D4.
*/
#include <Arduino.h>
#include <TM1637Display.h>

#define PIN_CLK  D3
#define PIN_DIO  D4

TM1637Display disp(PIN_CLK, PIN_DIO);

const uint8_t ALL_ON[3] = {0x7F, 0x7F, 0x7F};
const uint8_t ALL_OFF[3] = {0x00, 0x00, 0x00};

void setup() {
  disp.setBrightness(0x07, true);
}

void loop() {
  disp.setSegments(ALL_ON, 3, 0);
  delay(1000);
  disp.setSegments(ALL_OFF, 3, 0);
  delay(500);
}
