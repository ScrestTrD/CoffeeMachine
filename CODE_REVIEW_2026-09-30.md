# CoffeeMachine — audit baseline v6

Snapshot audit v6 trước sửa. Trạng thái source v7 nằm trong docs/QA_STATUS.md.

Ngày: 2026-09-30. Phạm vi: đọc toàn bộ code/tài liệu trong repository, xác định firmware chính thức, gom code thử nghiệm vào test/. Không sửa thuật toán production, không nạp ESP8266, không chạy thiết bị.

## Nguồn và baseline

- Repository thực tế: /mnt/pc-dev/CoffeMachine (khác đường dẫn /mnt/pc-dev/CoffeeMachine trong yêu cầu).
- Firmware chính thức: CoffeeMachine/CoffeeMachine.ino, FW_VERSION=6, 1422 dòng.
- Đường dẫn Windows do chủ dự án cung cấp: H:\Develop\CoffeMachine\CoffeeMachine\CoffeeMachine.ino.
- Gateway mount: /mnt/pc-dev = //192.168.1.171/Develop (CIFS). Đây là bản trong share Develop tương ứng thư mục chủ dự án chỉ định; chưa xác minh riêng ánh xạ ký tự ổ H: trên Windows.
- Origin: https://github.com/SlncTrZ/CoffeeMachine.
- HEAD lúc kiểm tra: 953cb1db2f7386e4e2084d357cfeebb29996c5e6.
- Git ban đầu báo 14 file modified. git diff --ignore-space-at-eol --exit-code trả 0: khác biệt hiện có chỉ là whitespace cuối dòng/CRLF. Không reset hoặc chuẩn hóa chúng.
- SHA256 firmware trước/sau tổ chức lại: 9e356046bbf7e9cb627d5827ac08063835761259a65b48cc2c60edce85b0777c.

## Các lỗi baseline v6 (trước sửa v7)

Đây là lỗi baseline v6, không phải regression do chuyển thư mục.

| ID | Mức | Vị trí | Điều kiện / hậu quả | Kiểm chứng / hướng sửa |
|---|---|---|---|---|
| CM-001 | Cao | CoffeeMachine.ino:850, 888, tick() | State không hợp lệ gọi failClosed(), chuyển FAULT_LATCHED nhưng fault vẫn NONE. heatingPermission() chỉ loại BOOT_SAFE; lệnh chốt SSR cuối tick có thể bật lại heater. tickFaultLatched() tắt SSR nhưng lệnh chốt sau FSM tiếp tục bật lại. | Mô phỏng state=255, NTC=80°C, setpoint=97.5°C: FAULT_LATCHED nhưng SSR=HIGH. Cần cấm heating trong FAULT_LATCHED/unknown state hoặc latch mã fault hợp lệ. |
| CM-002 | Cao | CoffeeMachine.ino:1195 tickPresetRecordActive() | Không kiểm tra MAX_DOSE_TIME_MS trong ghi preset. Không bấm dừng thì bơm/van chạy không giới hạn; có thể lưu preset lớn hơn 60s mà chạy preset chỉ chiết tối đa 60s. | Mô phỏng 113s từ recValveOpenMs: vẫn PRESET_RECORD_ACTIVE, bơm/van HIGH. Cần timeout và chính sách lưu/hủy rõ ràng. |
| CM-003 | Vừa | CoffeeMachine.ino:995, 1135, 1177 | READY_IDLE không kiểm tra lại nhiệt giảm. Nhánh no-preset hoặc hủy PRESET_RECORD_READY dùng enterIdle(READY_IDLE) trực tiếp. LED READY có thể báo sai. | Mô phỏng READY với NTC=80°C/setpoint=97.5°C vẫn LED RUN HIGH; hủy record-ready cũng vào READY. Cần tính readiness từ nhiệt khi vào idle và mỗi tick. |
| CM-004 | Vừa | CoffeeMachine.ino:1252, 1295 | Giữ RUN 2s chỉ được xử lý trong RUN_ACTIVE, không trong preamble 6s. Tài liệu nói abort bằng RUN trong mọi thời điểm là sai. | Đọc code: tickRunPredelay() chỉ xét SET; RUN held chỉ xét trong tickRunActive(). Cần thống nhất hành vi hoặc ghi rõ phạm vi. |
| CM-005 | Vừa | CoffeeMachine.ino:873 và 907 | NTC_TIMEOUT_MS chỉ dùng lúc BOOT_SAFE; không có watchdog tuổi mẫu NTC sau startup. Tài liệu “2s không mẫu tốt → fault” mô tả quá rộng. Mẫu ADC lỗi vẫn được latch E1 ở lần publish kế tiếp. | Kiểm tra tĩnh; chưa tái hiện sensor ngừng publish trên thiết bị. Cần timestamp cho mẫu hợp lệ nếu muốn cam kết timeout runtime. |
| CM-006 | Vừa | CoffeeMachine.ino:555, 577 | CRC hợp lệ không bảo đảm giá trị cấu hình hợp lệ: NaN setpoint lọt qua hai phép so sánh; R0/Beta không được kiểm tra miền. load() không snap 0.5 như logic.md nói. save() bỏ qua kết quả EEPROM.commit(). | Kiểm tra tĩnh; cần validate semantic và báo lỗi commit. Thay NTC_DEFAULT_R0/BETA trong code không thay config v3 hợp lệ đã lưu. |
| CM-007 | Vừa | CoffeeMachine.ino:888 | Ngưỡng 145°C dùng nhiệt đã lọc alpha=0.25 và đã bù -18°C; không phải cắt tức thời bằng rawTemp. Bơm vẫn chạy, chỉ SSR bị cắt. | Kiểm tra tĩnh; độ trễ và độ đúng nhiệt thực cần đo trên thiết bị. Không coi kiểm tra host là chứng nhận nhiệt/hydraulic. |

## Ma trận kiểm tra tài liệu

| File | Kết quả đối chiếu |
|---|---|
| COFFE_README.md | Đã đọc toàn bộ. Spec lẫn lịch sử PID/xung/AP dù có nhãn. §5.3 nói prime hoàn tất mới cho heat nhưng code heat trong prime; §8.2 mô tả heating display 0000 thay vì 8888 nháy; §8.4 nói RUN abort mọi lúc; §11–12 cam kết fail-closed mạnh hơn CM-001; §13 còn TBD max-dose/flow-edge dù code dùng 60s/FALLING. Ghi chú audit bổ sung để phân biệt yêu cầu với implementation. |
| HUONG_DAN_VAN_HANH.md | Đường dẫn firmware dư một cấp CoffeeMachine; heating display sai; hướng dẫn sửa R0/Beta chưa tính giá trị EEPROM đang lưu và offset -18; abort chưa phân biệt RUN_ACTIVE với preamble. Đã sửa thông tin vận hành và đường dẫn test. |
| CoffeeMachine/readme.md | Nêu quá 145°C cắt cả bơm/van/SSR rồi lại nói chỉ SSR. 60s không bao phủ record; timeout 2s chỉ boot. Đã sửa mô tả và liên kết báo cáo. |
| CoffeeMachine/hardware.md | D5=GPIO14 van, D6=GPIO12 bơm đúng; comment trong .ino ghi ngược GPIO12/14, macro D5/D6 vẫn đúng. Tham chiếu line 14–25 cũ. Không có phép đo điện mới trong lượt này. Đã sửa tham chiếu và phạm vi timeout. |
| CoffeeMachine/instruction.md | Startup/preset/clean/edit nhìn chung khớp. RUN abort chỉ RUN_ACTIVE; SSR còn bị cấm khi NTC lỗi/fault chứ không chỉ quá nhiệt. Ghi preset chưa có timeout. Đã bổ sung giới hạn hiện tại. |
| CoffeeMachine/logic.md | Đã đọc toàn bộ. Chưa đúng: load snap 0.5, timeout runtime; hướng phục hồi flowmeter còn nhắc feedForward đã xóa. Đã sửa các mô tả và bổ sung audit. |
| Preview_AP.html | Mockup tĩnh, POST /save và /cal* không có handler trong firmware. Range 90–100/R0=100k là thiết kế AP cũ; giữ nguyên mockup và ghi rõ trạng thái trong README. |
| test/4X 7 Segment.txt | Code test display, không phải tài liệu thuần. COMMON_CATHODE=1 thực tế đảo bit; các comment active-HIGH/common-cathode mâu thuẫn. setup() self-test dùng byte không đảo, khác displayNumber(). Giữ nguyên để không thay kết quả test cũ. |
| test/NTC_Temperature.txt | Code calibration test Serial, có delay và lấy một ADC cho c/h; mặc định R0=100k, không offset -18 nên khác production. Không phải bộ hiệu chuẩn đã nghiệm thu. Giữ nguyên code. |

## Ma trận code thử nghiệm

| Đường dẫn mới | Vai trò / giới hạn |
|---|---|
| test/DisplayDigitTest/DisplayDigitTest.ino | Thực tế nháy cả 4 số 8888, không test từng digit như header ghi. Serial dùng GPIO TX/RX. |
| test/Flowmeter_Test/Flowmeter_Test.ino | Đếm xung/đổi lít theo 5880; không phải thuật toán dosing production. Công thức scaled cast uint16_t trước khi saturate có thể wrap ở lượng lớn; comment RUN nối GND mâu thuẫn active-HIGH. |
| test/NtcTempMonitor/NtcTempMonitor.ino | R0/Beta/offset/filter hiện khớp defaults v6; header/log còn ghi v4. Không tái tạo toàn bộ fault-range hoặc config EEPROM của production; không phải kết quả đo đã hiệu chuẩn. |
| test/TM1637Blink888/TM1637Blink888.ino | Test module TM1637 3 digit, khác display 4x74HC595 production. |
| test/TM1637Test/TM1637Test.ino | Test module TM1637, khác production. |
| test/Test_4x7Seg/Test_4x7Seg.ino | Test đếm display 74HC595; tên COMMON_CATHODE gây nhầm vì nhánh 1 đảo bit. |
| test/4X 7 Segment.txt, test/NTC_Temperature.txt | Hai sketch tham chiếu gốc, giữ tên .txt và nội dung. |
| test/host_review/ | Reproducer 4 lỗi CM-001/002/003 bằng mock Arduino/EEPROM/display; không mô phỏng phần cứng ESP8266. |

## Kiểm chứng đã thực hiện

- Đọc 1422 dòng firmware và tất cả 9 file tài liệu/reference/mockup + 6 sketch phụ + .gitignore.
- Host g++ -std=c++11 biên dịch harness với bản firmware copy nguyên byte: exit 0, không stderr.
- Bốn ca repro đều trả 1 (lỗi tái hiện): unknown_state_restores_SSR, recording_continues_after_113s_extraction, READY_remains_at_80C_setpoint_97_5, record_cancel_returns_READY_at_80C.
- Exit 0 của reproducer nghĩa là cả 4 lỗi hiện có được tái hiện, KHÔNG phải firmware đã pass QA.
- SHA256 xác nhận 8 code test được di chuyển không đổi byte; firmware chính thức không đổi byte.
- Gateway không có arduino-cli hoặc pio trong PATH. Chưa compile/link bằng ESP8266 core thật, kiểm tra IRAM ISR, nạp chip, đo nhiệt/nguồn, test thủy lực hoặc xác minh binary đang chạy.

## Thứ tự xử lý đề xuất

1. CM-001 và CM-002: chốt nhánh fault luôn tắt heater và timeout recording.
2. CM-003/004/005: readiness, abort, tuổi mẫu runtime.
3. CM-006: validate config, calibration migration và commit failure.
4. CM-007: kiểm tra cutoff với raw/filtered và hiệu chuẩn thực tế.
5. Build ESP8266 thật và bench tests; chỉ đóng lỗi khi có bằng chứng tương ứng.

## Cập nhật triển khai v7

CM-001…CM-007 đã sửa phần logic và có host assertions; 70/70 case pass thường + ASan/UBSan, 5/5 core cases fail trên snapshot v6. Target compile/link và ISR IRAM G09 pass. G10 chưa chạy; calibration/thermal/power-loss thực vẫn cần bằng chứng. Xem [QA_STATUS](docs/QA_STATUS.md) và [QA_GATES](docs/QA_GATES.md). Các khẳng định hash không đổi ở phần audit áp dụng v6 trong lượt review ban đầu, không áp dụng bản v7 đã sửa.
