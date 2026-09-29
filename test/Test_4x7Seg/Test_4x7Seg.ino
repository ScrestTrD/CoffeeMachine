/*
  Test_4x7Seg — NodeMCU ESP8266 + 4-digit 7-seg + 4x74HC595 (3-wire)
  Test chạy từ 0000 -> 9999, giữ số 0 ở đầu.
  Dựa theo: 4X 7 Segment.txt (2026-09-02)

  WIRING (NodeMCU -> module):
    D2 (GPIO4) -> SDI  (data)
    D3 (GPIO0) -> SCLK (shift clock)
    D4 (GPIO2) -> LOAD (latch)
    3V3 -> VCC+, GND -> GND-

  Thư viện: https://github.com/Simsso/ShiftRegister74HC595
*/

#include <ShiftRegister74HC595.h>

// ------------------- PIN -------------------
#define SDI   D2
#define SCLK  D3
#define LOAD  D4

// 4 chip nối tiếp, mỗi số 1 chip
ShiftRegister74HC595<4> sr(SDI, SCLK, LOAD);

// 1 = common-cathode. Đổi 0 nếu module common-anode.
#define COMMON_CATHODE 1

// Mã segment 0..9, bit0=A bit1=B bit2=C bit3=D bit4=E bit5=F bit6=G bit7=DP
const uint8_t segCode[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66,   // 0 1 2 3 4
  0x6D, 0x7D, 0x07, 0x7F, 0x6F    // 5 6 7 8 9
};

// Hiển thị 0..9999, luôn đủ 4 số (0000..9999)
void displayNumber(uint16_t value) {
  uint8_t vals[4];
  vals[0] = segCode[(value / 1000) % 10];  // nghìn (trái nhất)
  vals[1] = segCode[(value / 100) % 10];   // trăm
  vals[2] = segCode[(value / 10) % 10];    // chục
  vals[3] = segCode[value % 10];           // đơn vị (phải nhất)

#if COMMON_CATHODE == 1
  for (int i = 0; i < 4; i++) vals[i] = ~vals[i] & 0xFF;
#endif

  sr.setAll(vals);  // latch 1 lần, chống bóng ma
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\nTest_4x7Seg ready — dem 0000..9999"));

  // Self-test: 8888 (0.6s) -> 1234 (0.6s) -> tat
  uint8_t all[4] = {0x7F, 0x7F, 0x7F, 0x7F};
#if COMMON_CATHODE == 1
  for (int i = 0; i < 4; i++) all[i] = ~all[i] & 0xFF;
#endif
  sr.setAll(all);
  delay(600);

  displayNumber(1234);
  delay(600);
}

uint16_t value = 0;
unsigned long lastInc = 0;
const unsigned long STEP_MS = 250;  // tốc độ đếm, đổi tùy ý

void loop() {
  displayNumber(value);  // hiển thị tĩnh, không quét

  if (millis() - lastInc >= STEP_MS) {
    lastInc = millis();
    value = (value + 1) % 10000;
    // In ra Serial mỗi 10 số để đỡ spam
    if (value % 10 == 0) {
      Serial.printf("%04d\n", value);
    }
  }
}
