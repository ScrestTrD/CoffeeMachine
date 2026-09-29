# CoffeeMachine — trạng thái QA v7

Cập nhật 2026-09-30. Source firmware v7 đã sửa; chưa nạp thiết bị.

## Kết quả

| Gate | Trạng thái | Bằng chứng |
|---|---|---|
| G01–G08 | PASS — host logic | 70/70 assertions ở bản thường và ASan/UBSan; -Wall -Wextra -Wpedantic -Werror, không warning hoặc sanitizer error |
| Regression v6 | EXPECTED FAIL | 5/5 ca cốt lõi fail trên snapshot v6; các ca tương ứng pass trên v7 |
| G09 | PASS — target build | ESP8266 core 3.1.2, ShiftRegister74HC595 1.3.1; FQBN esp8266:esp8266:nodemcuv2; compile/link exit 0; FlowSensor::flowIsr tại 0x401000f0 trong .text1 (IRAM) |
| G10 | NOT RUN — hardware | Chưa đo wiring/reset/nguồn/NTC/calibration/heater/hydraulic/power-loss; chưa có firmware upload |

Firmware SHA256 của cả host và target evidence:
`001f5701e5f792207db7a8168d6778c3fd329cad4bebb05fe7aa7e5011764abf`

[Host JSON](host-gate-results.json), [target JSON](target-build-results.json), [gate definitions](QA_GATES.md), [bench cases](BENCH_TEST_PLAN.md). Số đếm là 70 case duy nhất chạy ở 2 chế độ, không phải 140 chức năng khác nhau.

## Findings

| Finding | Source v7 | Phần còn cần kiểm chứng |
|---|---|---|
| CM-001 | Fixed + G01 PASS: fault/state không bật lại SSR | Electrical OFF khi reset/unpowered: G10 |
| CM-002 | Fixed + G02 PASS: recording timeout 60s hủy/không ghi đè | Thủy lực và giới hạn liều thực: G10 |
| CM-003 | Fixed + G03 PASS: READY/cancel/no-preset tính lại nhiệt | Nhiệt thật/readiness UX: G10 |
| CM-004 | Fixed + G04 PASS: RUN hold dừng preamble/active | Nút thực/debounce khi có EMI: G10 |
| CM-005 | Fixed + G05 PASS: watchdog mẫu runtime/boot và rollover | NTC/EMI/reset trên thiết bị: G10 |
| CM-006 | Fixed + G06 PASS: semantic validation, snap, checked saves, restart/commit counts | Flash power-loss/erase failure thực: G10 |
| CM-007 | Fixed phần logic + G07 PASS: cutoff xét raw đã bù và filtered | Calibration/overshoot/độ trễ vật lý vẫn OPEN trong G10 |

## Phạm vi và giới hạn

- Host mock dùng đúng chữ ký ESP8266 EEPROM begin void/length/getConstDataPtr/commit bool. Kiểm tra buffer setup phát hiện size/pointer lỗi; upstream begin không expose flashRead status. Không tuyên bố phát hiện đầy đủ mọi lỗi đọc flash.
- Mock commit-failure giữ flash cũ để kiểm tra đường lỗi/reboot. Flash ESP8266 thật có erase trước write; thất bại giữa erase/write hoặc mất điện có thể mất record. CRC/defaults xử lý record hỏng, chưa có dual-slot transactional storage. Không coi restart mock là nghiệm thu power-loss thực.
- Defaults/calibration không được đổi âm thầm. Config v3 hợp lệ giữ R0/Beta/preset; invalid config hoặc preset >60s về defaults.
- Target toolchain tạm dùng Arduino CLI 1.5.2-rc.1, core/library nêu trên. Metadata/log phiên bản lưu trong target JSON. Compile có cảnh báo môi trường thiếu HOME và SyntaxWarning từ elf2bin.py của core; không có lỗi compile/link. Không sửa môi trường hay toolchain dùng chung.
- Build dùng RAM 28652/80192; IRAM tổng 60051/65536 (bao gồm cache 32768), flash 241368/1048576. IRAM còn khoảng 5.5KB; thay thư viện/flags cần build lại.
- [Audit v6](../CODE_REVIEW_2026-09-30.md) là snapshot lịch sử trước sửa. Các hash “code không đổi”/lỗi hiện có trong snapshot chỉ áp dụng lượt audit v6, không áp dụng v7.

## Build artifacts đã lưu

Binary/ELF/map tại build/v7/ (gitignored). [Manifest/hash](build-artifacts.json). Binary SHA256: 2be2cc660732f44a754070d8b2150739dd0cc117b331081c162ad61b5cf6336e. Chưa upload thiết bị.

## API tham chiếu

Đã đối chiếu [EEPROM.h](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/EEPROM/EEPROM.h) và [EEPROM.cpp](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/EEPROM/EEPROM.cpp) của ESP8266 core; compile thật với 3.1.2 là bằng chứng tương thích dùng trong G09.

G10 chưa chạy vì PC không kết nối ESP, theo xác nhận của chủ dự án ngày 2026-09-30. Không có thao tác upload hoặc điều khiển thiết bị trong lượt commit/push.

Đã xác minh repository trên PC .171 tại H:\Develop\CoffeMachine khi chuẩn bị commit/push. Remote main trước push: 953cb1db2f7386e4e2084d357cfeebb29996c5e6.
