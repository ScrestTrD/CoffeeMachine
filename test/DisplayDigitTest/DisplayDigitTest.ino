/*
  DisplayDigitTest — test LED 7 doan 4x74HC595 rieng tung digit.
  Cung chan + cuc tinh dao nhu CoffeeMachine.ino.
  Mo Serial 115200 de xem dang test digit nao.
*/
#include <Arduino.h>
#include <ShiftRegister74HC595.h>

#define PIN_SDI  D2
#define PIN_SCLK D3
#define PIN_LOAD D4
#define DISPLAY_INVERT_SEGMENTS 1

ShiftRegister74HC595<4> sr(PIN_SDI, PIN_SCLK, PIN_LOAD);

// Raw (truoc dao): 0x80 = sang A-G (tat DP), 0xFF = tat het
static void showRaw(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  uint8_t out[4] = {b0, b1, b2, b3};
  sr.setAll(out);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("DisplayDigitTest start"));
}

void loop() {
  // Nhap nhay 8888 loop vo han: sang 1s / tat 0.5s
  showRaw(0x80, 0x80, 0x80, 0x80);
  delay(1000);
  showRaw(0xFF, 0xFF, 0xFF, 0xFF);
  delay(500);
}
