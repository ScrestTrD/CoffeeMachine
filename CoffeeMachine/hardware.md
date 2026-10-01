# Hardware — CoffeeMachine v15

Board NodeMCU ESP8266. Pin map mô tả source và quyết định wiring được ghi trước đây; chưa có phép đo phần cứng mới trong lượt sửa v15. Xem [bench G10](../docs/BENCH_TEST_PLAN.md).

## Pin map

| NodeMCU | GPIO | Hướng | Mức ON / chức năng |
|---|---|---|---|
| D2 | 4 | OUT | 74HC595 SDI |
| D3 | 0 | OUT | 74HC595 SCLK; phải HIGH khi boot |
| D4 | 2 | OUT | 74HC595 LOAD; phải HIGH khi boot |
| A0 | ADC | IN | NTC leg dưới; source dùng full-scale 3.3V theo bo hiện tại |
| D6 | 12 | OUT | Bơm active-HIGH |
| D5 | 14 | OUT | Van active-HIGH |
| D7 | 13 | OUT | SSR active-HIGH, steady 3.3V logic, không PWM |
| D0 | 16 | IN | SET LOW=nhấn; pull-up ngoài 10k |
| D8 | 15 | IN | RUN HIGH=nhấn; pull-down ngoài 10k, boot LOW |
| D1 | 5 | ISR IN | Flowmeter INPUT_PULLUP/FALLING; diagnostic only |
| RX | 3 | OUT | LED SET HIGH qua R 1k |
| TX | 1 | OUT | LED RUN HIGH qua R 1k |

Không dùng GPIO6–11 (flash). Không đổi D8 sang INPUT_PULLUP hoặc giữ RUN khi reset. D0 dùng pull-up ngoài; firmware dùng INPUT.

## Cụm và nguồn

- Display 4x74HC595, segment active-LOW, bytes đảo trong production. TM1637 trong test/ là module khác. Các tên COMMON_CATHODE trong test cũ không mô tả chính xác polarity code.
- Divider 3.3V → 10k → A0 → NTC → GND. Defaults R0 185000Ω, Beta 4890K (fit 2 điểm NTC mới 2026-09-30 21:06) + offset +15 (v11, 2026-09-30 22:39); R0/Beta trong EEPROM v5 hợp lệ có thể khác defaults.
- ADC rail checks ≤0.02V hoặc ≥3.28V; plausibility −40…300°C trước offset. Median 7 mẫu/6ms và lọc alpha0.25. NTC publish invalid latch E1; chưa publish boot hoặc không có mẫu hợp lệ mới runtime 2s latch E3.
- Cutoff SSR xét raw đã bù và filtered >145°C; không phải thermal fuse, không bảo đảm nhiệt boiler thật. Overtemp tự hồi khi cả hai ≤145; bơm/van không bị cắt bởi riêng ngưỡng này.
- Chu trình pha yêu cầu SSR ON cả khi bơm OFF trong soak; prime vẫn dùng pump-force. Nhưng BOOT_SAFE/fault/state lạ/mẫu stale/cutoff khóa heater. Unknown FSM E8, storage buffer/commit failure E6: mọi actuator OFF.
- SSR phải hỗ trợ kích 3.3V hoặc có driver phù hợp nguồn/input. Relay phải xác nhận active-HIGH. Bias phần cứng giữ mọi actuator OFF khi reset/unpowered; nguồn phải đủ khi ba tải cùng ON, mass low-voltage chung.
- Mains cách ly low-voltage; sensor phải theo được nhiệt heater ở idle lẫn flowing. Thermal fuse và thermostat độc lập cần được xác minh trên thiết bị.
- Flowmeter không quyết định liều, không có no-flow abort. Xác minh output điện (không đưa 5V push-pull vào GPIO).
- GPIO1/3 dùng LED nên production không Serial.begin(). USB-UART có thể chọi RX; tách UART/LED đúng khi service. Test Serial không phải chế độ vận hành máy.

Heater1400W theo chủ máy cung cấp ngày2026-10-01; chưa đo công suất/độ trễ NTC/overshoot trong lượt này. Hồi nhiệt v15 theo NTC sau lọc tới setpoint, không dự đoán nhiệt tích trữ.

## Kiểm chứng

[G09](../docs/QA_STATUS.md) đã compile/link với core 3.1.2 và kiểm tra ISR IRAM. G10 chưa đo wiring, nguồn, SSR trigger, calibration, overshoot, hydraulic hoặc power-loss; không coi source defaults là số đo mới.


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.
