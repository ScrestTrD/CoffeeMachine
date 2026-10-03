# CoffeeMachine — đặc tả hiện hành

Cập nhật 2026-10-01. Source v15 đã triển khai; trạng thái thực hiện và kiểm chứng nằm trong [QA_STATUS](docs/QA_STATUS.md). Tài liệu này thay đặc tả trộn PID/xung/AP cũ; [audit v6](CODE_REVIEW_2026-09-30.md) giữ lịch sử findings.

## Hardware

NodeMCU ESP8266; [pin map và wiring](CoffeeMachine/hardware.md) là tham chiếu. D6/GPIO12 bơm, D5/GPIO14 van, D7/GPIO13 SSR, active-HIGH. D0 SET active-LOW với pull-up ngoài; D8 RUN active-HIGH với pull-down ngoài. Display 4x74HC595 active-LOW trên D2/D3/D4; không phải TM1637. GPIO1/3 là LED, production không gọi Serial.begin().

Không dùng GPIO6–11. Boot cần GPIO0/2 HIGH, GPIO15 LOW. Bias phần cứng phải giữ relay/SSR OFF khi MCU reset/unpowered. SSR trigger 3.3V, nguồn, isolation, NTC location, thermal fuse và thủy lực cần nghiệm thu G10; firmware không thay bảo vệ nhiệt độc lập.

## Nhiệt

- Thermostat ON dưới setpoint−0.5°C, OFF từ setpoint+0.5°C; không PWM/PID.
- Default 97.5°C; miền 90–140°C; snap 0.5°C, nút edit bước 1°C.
- NTC defaults R0=185000Ω, Beta=4890K (fit 2 điểm NTC mới 32/86°C, 2026-09-30 21:06), divider 10k, A0 full-scale giả định theo bo này 3.3V.
- 7 ADC samples cách 6ms, median rồi Beta (R0 185000/Beta 4890) + offset +15 (v11, đọc thấp đều 15); alpha=0.25 cho nhiệt điều khiển.
- **Không còn cắt cứng mềm theo nhiệt độ (v17).** Bảo vệ quá nhiệt bằng cầu chì nhiệt/thermostat phần cứng độc lập; firmware chỉ thermostat quanh setpoint.
- Trong pha preset/ghi preset, SSR được yêu cầu ON suốt wet/soak/press/extraction, kể cả bơm OFF trong soak. Xả cũng yêu cầu đun suốt lúc chạy.
- Kết thúc/dừng/hủy/timeout chu trình: NTC sau lọc <setpoint thì hồi nhiệt; ≥setpoint thì SSR OFF ngay. Sau pha giữ giám sát: filtered < set bật SSR ngay, filtered ≥ set tắt ngay. Chỉ trả về thermostat ±0,5°C sau các mẫu mới liên tiếp trong [set, set+0,5°C] đủ 3 giây. Ra khỏi vùng, đổi set hoặc bị bảo vệ ngắt thì tính lại; mẫu cũ không kéo dài xác nhận. Đây là thời gian quan sát, không ép đun thêm. Không có khoảng ép đun cố định hoặc mục tiêu +1/+2°C.
- Safety permission luôn thắng: fault, freshness, BOOT_SAFE/state lạ. Prime giữ pump-force/thermostat; hủy chọn trước khi pha không arm hồi nhiệt.
- Mẫu đầu phải hợp lệ trước prime. Không có publish lúc boot trong 2s: E3; ADC hở/chập/ngoài −40…300°C trước offset: E1. Sau startup, 2s không có mẫu hợp lệ mới: E3.
- State lạ: E8; fault latch luôn tắt mọi actuator đến reset.

## Trình tự và UI

| State/chế độ | Hành vi |
|---|---|
| BOOT_SAFE | Tất cả OFF, hiện FW_VERSION; đợi NTC và storage init |
| STARTUP_PRIME | 5s bơm ON/van đóng; gia nhiệt được phép, 8888 và 2 LED nháy 2Hz |
| HEATING_IDLE | 8888 nháy 2Hz, RUN LED 1Hz |
| READY_IDLE | 0000, RUN steady; tính lại readiness mỗi tick |
| READY criteria | NTC hợp lệ/còn mới, không fault, control temp ≥setpoint−2°C |
| Chọn pha | RUN từ idle, SET đổi preset hợp lệ, RUN xác nhận; không có preset hiện no |
| Preamble | 2s van+bơm ON → 2s bơm/van OFF → 2s bơm ON/van đóng; SSR vẫn yêu cầu ON qua safety |
| RUN_ACTIVE | Thời gian liều tính từ mở van chiết; màn hình tính từ RUN bắt đầu |
| Dừng pha | SET press hoặc RUN giữ 2s, cả preamble và chiết |
| Ghi preset | SET giữ 3s → RUN đổi slot → SET chốt → RUN bắt đầu → RUN dừng/lưu; SET hủy |
| Recording limit | 60s chiết: dừng/hủy, không ghi đè; 0s không lưu |
| CLEAN_FLUSH | SET idle nhấn-nhả ≤700ms; bơm+van ON, 2 LED steady; SET nhấn-nhả 100ms–<3s dừng; tự dừng 60s |
| SETPOINT_EDIT | SET+RUN 5s rồi nhả; RUN +1/SET −1; giữ cả hai 300ms lưu; bỏ 20s hủy |

Cho phép bắt đầu liều từ heating idle, không có ready gate bắt buộc. Idle sau hủy/xong tính lại nhiệt. Queue và guard release tránh nút dừng tiếp tục kích xả ở idle. Các timer phải dùng unsigned subtraction để chịu millis rollover; timestamp bằng 0 không được dùng để kết luận “chưa chiết”.

Nếu vừa kết thúc pha mà nhiệt lọc còn cao, SSR OFF theo số đo đã duyệt. Trong giám sát sau pha, nhiệt lọc dưới set thì bật lại ngay; không bảo đảm loại bỏ độ trễ nhiệt của cảm biến. Heater1400W (chủ máy cung cấp) có thể tiếp tục tăng nhiệt sau SSR OFF. Xác minh thực tế bằng G10.

## EEPROM

Giữ layout/version 5, magic/size/CRC. Config hợp lệ còn phải có setpoint hữu hạn trong miền, R0 1000–1000000Ω và Beta 800–6000K hữu hạn, lastPreset 0–2, valid flag 0/1; preset hợp lệ từ 1–60s. Đây là miền kiểm tra input, không chứng nhận calibration.

Config invalid/legacy khác layout/preset vượt miền: defaults và không tự ghi flash. Config v5 hợp lệ giữ calibration và preset; load snap setpoint 0.5. Đổi defaults source không tự thay config đang lưu — ngoại lệ một lần v9 (lên version 5 ép defaults vì R0/Beta cũ đã biết sai). Khi hiệu chuẩn lại phải đồng bộ R0/Beta có chủ đích.

EEPROM buffer setup (length/pointer) hoặc commit failure: E6, mọi actuator OFF. begin() của core trả void và không expose flashRead status, nên không cam kết phát hiện mọi lỗi đọc flash. Chỉ commit khi xác nhận thay đổi; hoàn tất pha cùng lastPreset không ghi lại. Không write mỗi loop.

## Mã lỗi

| Mã | Nghĩa |
|---|---|
| E1 | NTC ADC/giá trị không hợp lệ |
| E3 | Timeout mẫu NTC boot/runtime |
| E6 | EEPROM init/save thất bại |
| E8 | FSM state không hợp lệ |
| no | Không có preset hợp lệ; không phải fault |

Fault không tự clear khi UI đổi. Không có cắt cứng mềm theo nhiệt độ (v17); quá nhiệt do cầu chì nhiệt/thermostat phần cứng xử lý.

## Gates

[G01–G08](docs/QA_GATES.md) kiểm chứng logic bằng host mocks. G09 compile/link thật với ESP8266, kiểm tra IRAM; G10 đo wiring/reset, sensor, hydraulic, heater/overshoot/fuse, persistence/power loss. Không đóng G09/G10 bằng host test. Các sketch linh kiện trong test/ không tự tắt đầy đủ actuator và không phải firmware vận hành.


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.
