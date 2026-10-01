# Code test CoffeeMachine

Firmware chính thức: ../CoffeeMachine/CoffeeMachine.ino. Các test tách riêng trong thư mục này.

## Gates tự động

```sh
python3 test/run_gates.py --report docs/host-gate-results.json
```

Runner compile production source với mock I/O/EEPROM, chạy 105 assertions ở bản thường và AddressSanitizer/UBSan. Compiler flags có -Wall -Wextra -Wpedantic -Werror. Exit 0 = assertions an toàn pass; khác 0 = fail. Không còn semantics “lỗi tái hiện = pass” của reproducer cũ.

## Regression nhiệt trên cùng source

Test mới trong host_review/heat_recovery_cases.h được include vào suite production. Bao phủ H01–H19: giám sát 3s, reset ổn định, mẫu cũ/gap, rollover, soak, các exits, filtered setpoint boundary, xoá thermostat latch, cutoff/faults, new cycle/edit/rollover và SSR trước flash commit. Mock EEPROM.onCommit chỉ quan sát trạng thái output tại ranh giới synchronous flash write.

Để chạy lại đối chứng snapshot trước sửa (còn trong workspace build, gitignored):

```sh
python3 test/run_gates.py --firmware build/v14-pre-monitor.ino --report docs/pre-monitor-reproduction.json
```

Báo cáo v11 101 case/26 fail là lịch sử. Hiện tại chạy105 case; snapshot build/v14-pre-monitor.ino tái hiện lỗi giám sát (exit1), source mới pass (exit0). Host report hash đúng source được chọn bởi --firmware. JSON before/after lưu trong docs; đây là regression sensitivity, không phải phép đo nhiệt thật.

--baseline vẫn dành cho5 case lịch sử v6 nếu có snapshot v6 riêng; không dùng cho v11 và không phải gate bắt buộc sau khi lịch sử repository đã squash.

## Target build G09

Cài toolchain riêng phù hợp, ESP8266 core 3.1.2 và ShiftRegister74HC595 1.3.1; lượt này dùng cấu hình tạm, không sửa toolchain chung. Chạy:

```sh
python3 test/build_esp8266.py --cli /path/to/arduino-cli --config-file /path/to/arduino-cli.yaml --objdump /path/to/xtensa-lx106-elf-objdump --report docs/target-build-results.json
```

FQBN default esp8266:esp8266:nodemcuv2. Runner không upload; kiểm tra compile/link và ISR symbol IRAM. Exit 77 = NOT RUN/PARTIAL (thiếu CLI/objdump), không phải pass. Gate G10 thực hiện theo [bench plan](../docs/BENCH_TEST_PLAN.md).

## Sketch linh kiện cũ

| Đường dẫn | Mục đích |
|---|---|
| DisplayDigitTest/ | Nháy 8888 trên 4x74HC595, header “từng digit” không đúng với loop hiện có |
| Test_4x7Seg/ | Đếm 0000–9999; tên COMMON_CATHODE gây nhầm với inversion thực |
| Flowmeter_Test/ | Đếm xung/lít, không phải dosing production |
| NtcTempMonitor/ | Defaults/filter/offset NTC; không tái tạo config EEPROM custom hoặc toàn bộ fault logic |
| TM1637Test/, TM1637Blink888/ | Module TM1637 khác hardware production |
| 4X 7 Segment.txt | Sketch display tham chiếu gốc |
| NTC_Temperature.txt | Sketch calibration tham chiếu, defaults100k/no offset |
| host_review/heat_recovery_cases.h | Contract nhiệt hiện hành H01–H19 |
| host_review/ | Mock I/O/EEPROM/display và regression suite hiện hành |

6 sketch và 2 file .txt được chuyển nguyên byte từ root, xác nhận bằng RELOCATION_MANIFEST.json. Manifest là lịch sử di chuyển v7; upstream v9–v11 đã bổ sung/sửa một số sketch. Sketch linh kiện không được sửa trong lượt v14. Giữ thư mục trùng tên .ino khi mở Arduino IDE. File .txt cần copy vào sketch riêng để chạy.

Sketch linh kiện không tự tắt đầy đủ bơm/van/SSR; tách tải khi service. Serial chia sẻ TX/RX với LED production. Không dùng chúng thay firmware vận hành. [QA_STATUS](../docs/QA_STATUS.md) phân biệt host/target pass với G10 NOT RUN.
