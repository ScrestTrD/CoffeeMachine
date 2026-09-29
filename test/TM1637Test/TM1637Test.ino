#include <Arduino.h>
#include <TM1637Display.h>

#define PIN_CLK D3  // GPIO 0
#define PIN_DIO D4  // GPIO 2

TM1637Display display(PIN_CLK, PIN_DIO);

// Bật toàn bộ các đoạn (a, b, c, d, e, f, g, dp) thành hình số 8 có dấu chấm
const uint8_t all_on[] = { 0xff, 0xff, 0xff, 0xff };

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Khoi tao TM1637 ---");

    // Tham số 2 là true: BAT hien thi (Display ON)
    display.setBrightness(7, true); 

    // Ép sáng cả 4 digit để kiểm tra phần cứng
    display.setSegments(all_on);
}

void loop() {
    // Nhấp nháy số 8888 để kiểm tra tín hiệu liên tục
    display.setSegments(all_on);
    delay(500);
    display.clear();
    delay(500);
}