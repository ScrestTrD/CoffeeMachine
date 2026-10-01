> Lịch sử trước bản sửa giám sát sau pha. Hành vi hiện hành và bằng chứng mới: [QA_STATUS](QA_STATUS.md).

# Rà soát từng khối và thay đổi điều khiển nhiệt — 2026-10-01

Baseline: firmware v11, commit 1720787a4a719d70b4ed19b0362423ec81bf358d (GitHub đã squash lịch sử). Đã đọc toàn bộ 1.527 dòng firmware trước sửa, cùng host harness, mock EEPROM/display, runners và docs vận hành/gates. Bản triển khai dùng FW_VERSION=14 để phân biệt với các bản 12/13 đã thử trước đó; nhánh fix/brew-heat-recovery.

## Quyết định đã duyệt

- Pha preset và ghi preset yêu cầu đun suốt wet/soak/press/extraction, kể cả lúc bơm OFF trong soak. Xả vẫn yêu cầu đun khi đang chạy.
- Khi chu trình kết thúc hoặc bị dừng/hủy/timeout: dùng NTC sau lọc. Nếu < setpoint thì yêu cầu hồi nhiệt; nếu ≥ setpoint thì tắt ngay.
- Hồi nhiệt xét từng vòng lặp với mẫu mới nhất, không có thời gian ép đun cố định. Đạt hoặc vượt setpoint thì kết thúc hồi nhiệt và xoá latch ON cũ của thermostat.
- Sau hồi nhiệt, thermostat v11 hoạt động bình thường: ON dưới sp−0.5; OFF từ sp+0.5; giữ trạng thái trong deadband.
- Safety supervisor luôn ưu tiên. Không thay NTC R0/Beta/+15, E1/E3/E6/E8, cutoff145°C, EEPROMv5 hoặc trình tự bơm/van.
- Prime khởi động giữ hành vi v11, không được coi là pha. Thoát chọn preset/hủy trước khi bắt đầu pha không arm hồi nhiệt.

## Rà soát theo khối

| Khối | Kết quả đọc code và tác động |
|---|---|
| Pin map/constants | D7/GPIO13 SSR active-HIGH; bơm D6, van D5; ngưỡng đặt90–140, mặc định97.5. Giữ pin và ngưỡng. FW_VERSION lên14. |
| Fault codes | E1 invalid NTC/ADC hoặc nhiệt trước bù ngoài −40…300; E3 stale2s; E6 EEPROM; E8 state lạ. Giữ nguyên; không áp yêu cầu E1>180 của lượt v12 đã revert. |
| Display | 74HC595 active-LOW, 4 digit; refresh tối thiểu5ms khi commit frame; BOOT hiển thị version. Không đổi code hiển thị. |
| Buttons | Debounce25ms, queue6, press/release/hold. SET stop và RUN hold giữ luồng cũ. Không đổi gesture. |
| NTC | Median7 mẫu cách tối thiểu6ms → Beta → bù+15 → EMAalpha0.25; raw đã bù dùng cutoff. Không đổi filter/calibration; sửa comment cũ sai về offset. |
| Flow ISR | Counter diagnostic, IRAM ISR trực tiếp count++; không dùng dosing. Giữ code, xác minh IRAM bằng target build. |
| Persistence | Magic/version5/size/CRC + semantic checks; save bool; không auto-commit defaults. Giữ layout/calibration/preset. Sửa commentv3 thànhv5. |
| Thermostat | Giữ hysteresis±0.5°C. Thêm clearHeatRequest để kết thúc hồi nhiệt ở setpoint không bị latchON cũ kéo đun tới +0.5. |
| Safety supervisor | Permission allowlist, fault, freshness, finite raw/control và cutoff145. Cắt SSR trước FSM nếu không được phép; mọi lệnh đun sau FSM vẫn qua permission. |
| Brew/record FSM | Yêu cầu đun dựa trên trạng thái chu trình, không dựa riêng pumpOn. Phần soak chỉ bơm/van OFF. Trình tự2+2+2s và thời gian chiết giữ nguyên. |
| Exits/clean/edit | Chuyển khỏi chu trình thật mới arm hồi nhiệt. Bao phủ normal completion, SET/RUN abort, record save/cancel/timeout và clean stop/timeout. New cycle/fault xoá recovery cũ; edit dùng setpoint đã lưu mới nhất. |
| Output/EEPROM handoff | Bỏ ghi SSR theo trạng thái cũ ở đầu tick. Chốt output sau chuyển FSM; cuối pha quyết định SSR trước synchronous flash commit. Fail commit vẫn latchE6 và tắt mọi tải. |
| Setup/loop/LED | Output OFF khi begin, stateBOOT_SAFE và sensor gating giữ nguyên; recovery RAM reset khi begin. LED/UI và loop non-blocking giữ nguyên. |

## Các điểm sửa có bằng chứng

1. V11 tắt SSR trong soak nếu thermostatOFF, dù chu trình pha vẫn đang chạy.
2. V11 có thể không đun sau pha khi nhiệt lọc ở trong deadband nhưng vẫn dưới setpoint.
3. Nếu thermostat đã ON, chỉ bỏ cờ hồi nhiệt ở setpoint là chưa đủ: latchON có thể tiếp tục tới setpoint+0.5. V14 xoá latch khi bàn giao.
4. Đầu tick v11 có thể ghi mức SSR theo pump/state cũ rồi sửa lại cuối tick. V14 chỉ pre-cut khi không an toàn; yêu cầu mới tính sau FSM.
5. Cuối preset/record, output hồi nhiệt được áp trước EEPROM commit để flash write không trì hoãn hot-end cutoff.
6. Docs/QA trước sửa vẫn mô tả v7 và evidence v7 dù source đãv11; cập nhật docs hiện hành, giữ audit cũ như snapshot lịch sử.

## Kiểm chứng và giới hạn

Cùng101 case: v11 có26 case mới không đáp ứng contract, v14 pass101/101 ở normal và ASan/UBSan. 70 case cũ pass cả hai; không coi26 ca đó là26 lỗi độc lập. Bằng chứng: v11-heat-reproduction.json, host-gate-results.json. Tests mới nằm trong test/host_review/heat_recovery_cases.h.

G09 compile/link thật với ESP8266 core3.1.2, ShiftRegister74HC5951.3.1; metadata và ISR IRAM trong target-build-results.json. G10 chưa chạy, không upload.

Chủ máy cho biết heater1400W. Không thêm mô hình công suất, PID, đun bù vài giây, tắt sớm dự đoán hoặc đổi lọc. Nếu nhiệt lọc còn≥set lúc kết thúc pha, SSR sẽOFF đúng lựa chọn đã duyệt; khoảng chờ khi NTC sau đó mới giảm vẫn có thể tồn tại. Nhiệt vật lý còn tăng sau SSR OFF do nhiệt tích trữ chưa được host mocks mô phỏng. Không tuyên bố hết overshoot/hụt nhiệt thực hoặc cải thiện chất lượng chiết khi chưa có trace G10.
