# Hardware — CoffeeMachine v30

Board NodeMCU ESP8266 (ESP-12E). Pin map mô tả cấu hình phần cứng theo firmware v30. Xem kế hoạch kiểm thử bench [G10](../docs/BENCH_TEST_PLAN.md).

## Pin Map

| NodeMCU | GPIO | Hướng | Mức ON / Chức năng |
|---|---|---|---|
| D2 | 4 | OUT | 74HC595 SDI (dữ liệu nối tiếp hiển thị) |
| D3 | 0 | OUT | 74HC595 SCLK (xung clock dịch); phải ở mức HIGH khi boot |
| D4 | 2 | OUT | 74HC595 LOAD (xung chốt); phải ở mức HIGH khi boot |
| A0 | ADC | IN | NTC nhánh dưới cầu phân áp; thang đo 0–3.3V |
| D6 | 12 | OUT | Relay bơm (active-HIGH) |
| D5 | 14 | OUT | Relay van điện từ (active-HIGH) |
| D7 | 13 | OUT | SSR thanh nhiệt (active-HIGH), steady 3.3V logic (không PWM) |
| D0 | 16 | IN | Nút SET (nhấn = LOW); có điện trở kéo lên ngoài 10k |
| D8 | 15 | IN | Nút RUN (nhấn = HIGH); kéo xuống ngoài 10k, phải LOW khi boot |
| D1 | 5 | ISR IN | Flowmeter (INPUT_PULLUP, ngắt FALLING); chỉ dùng cho chẩn đoán |
| RX | 3 | OUT | LED SET (HIGH = sáng, qua điện trở 1k) |
| TX | 1 | OUT | LED RUN (HIGH = sáng, qua điện trở 1k) |

*Lưu ý GPIO*: Không sử dụng GPIO6–GPIO11 (kết nối SPI flash). Không đảo logic D8 sang INPUT_PULLUP hay nhấn giữ RUN khi cấp nguồn vì GPIO15 bắt buộc LOW lúc boot ESP8266.

## Cụm mạch và Nguồn điện (v30)

- **Hiển thị**: 4 IC 74HC595 ghép tầng điều khiển 4 led 7 đoạn. Firmware quét lại khung hình mỗi 5 ms (`DISPLAY_REFRESH_MS`) để chống sai lệch dữ liệu do sụt áp/nhiễu EMI từ bơm, van và SSR.
- **Mạch đo nhiệt NTC**:
  - Cầu phân áp: `3.3V → R_SERIES (10k) → A0 → NTC → GND`.
  - Tham số NTC mặc định: `R0 = 185000 Ω`, `Beta = 4890 K`, `offset = 0°C`.
  - Lấy mẫu: 9 mẫu cách nhau 1 ms (~9 ms/cửa sổ), lọc trung vị kết hợp bù trễ đạo hàm `leadT` (cửa sổ 500 ms, `NTC_LEAD_S = 10s`).
  - Debounce lỗi NTC: Cần 5 cửa sổ lỗi liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) mới kích hoạt báo lỗi E1/E3.
- **Bảo vệ nhiệt độ**:
  - **Trần an toàn phần mềm 120°C (`HEAT_CAP_C`)**: Tự động ngắt SSR khi nhiệt độ đo chạm 120°C ngoài chu trình pha.
  - **Cầu chì nhiệt độc lập**: Là lớp bảo vệ phần cứng tối cao chống cháy nổ nồi hơi.
- **SSR và Relay**:
  - SSR điều khiển thanh nhiệt 1400W, kích bằng mức logic 3.3V.
  - Relay bơm và van cần có mạch bias phần cứng giữ mức LOW khi MCU khởi động hoặc mất nguồn.
- **Giao tiếp UART / Telemetry**:
  - Bản firmware chính thức không gọi `Serial.begin()` do chân TX/RX dùng cho 2 đèn LED hiển thị.
  - Bản chẩn đoán `test/CoffeeMachineDebug` giải phóng 2 chân LED để bật UART truyền telemetry và nhận lệnh điều khiển cưỡng bức SSR.
