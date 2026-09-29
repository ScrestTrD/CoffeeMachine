# CoffeeMachine — gates và tiêu chí nghiệm thu

Cập nhật 2026-09-30. Hành vi mục tiêu: firmware v7. Các test host dùng firmware thật với mock I/O; gate hardware chỉ đóng khi có bằng chứng đo thực tế. G01–G09 đã PASS với bằng chứng trong QA_STATUS; G10 NOT RUN.

| Gate | Finding | Tiêu chí bắt buộc | Bằng chứng |
|---|---|---|---|
| G01 | CM-001 | BOOT_SAFE, FAULT_LATCHED, state lạ và fault NTC/storage luôn tắt SSR/bơm/van; không bật lại cuối tick | Host assertions trên output và lịch sử ghi chân |
| G02 | CM-002 | Ghi preset tự dừng ở 60s từ bắt đầu chiết, hủy không ghi đè; ca 59.999s/60s, rollover, SET/RUN | Host fake clock + EEPROM commit count |
| G03 | CM-003 | READY chỉ khi mẫu còn mới, không lỗi, nhiệt trong vùng sẵn sàng; nguội/hủy/no-preset tính lại readiness | Host FSM/button scenarios |
| G04 | CM-004 | SET hoặc RUN giữ 2s dừng cả preamble và RUN_ACTIVE; không tự chạy/xả lại sau dừng | Host debounce/gesture sequences |
| G05 | CM-005 | NTC boot và runtime timeout ở 2s; open/short/NaN latch; tính tuổi mẫu đúng khi millis wrap | Host sampling/fake clock |
| G06 | CM-006 | Kiểm tra CRC/layout/giá trị hữu hạn/miền/preset; load snap 0.5; commit failure E6; config v3 hợp lệ giữ calibration/preset; restart bền | Host flash mock, fault injection |
| G07 | CM-007 | Nhiệt chưa lọc đã bù vượt 145°C phải cắt SSR ở lần publish kế tiếp dù nhiệt lọc còn thấp; không cắt bơm/van vì overtemp; recovery cần cả raw và filtered ≤145 | Host thermal scenarios; độ trễ vật lý chờ G10 |
| G08 | UI/persistence | Startup 5s van đóng; wet/soak/press 2s mỗi pha; timer extraction riêng; clean timeout; edit save/timeout; không commit EEPROM liên tục | Host regression |
| G09 | Build ESP8266 | Compile/link sketch v7 với core/library thật; kiểm tra ISR nằm IRAM; lưu FQBN và phiên bản dependencies | Target compiler output + map |
| G10 | Hardware | Wiring/default OFF/reset; boot straps; NTC đa điểm; SSR trigger, overshoot/cutoff, pump/valve hydraulics; power-loss/fuse | Bench log và số đo |

## Quy tắc thay đổi

- Ghi preset timeout: hủy, không tự lưu một liều chưa được RUN xác nhận.
- Giữ config EEPROM version 5/layout; chỉ nhận preset 1–60s. Config CRC/semantic không hợp lệ trở về defaults, không tự ghi flash ở boot.
- R0/Beta hợp lệ trong EEPROM được giữ nguyên. Đổi defaults trong source không được âm thầm ghi đè hiệu chuẩn của chủ máy — ngoại lệ một lần v9 (lên version 5 ép defaults R0 185000/Beta 4890 vì giá trị cũ đã biết sai theo 2 điểm đo thật).
- E6 dùng cho lỗi EEPROM begin/save; E8 cho state không hợp lệ. Fault latch tới reset.
- 145°C là ngưỡng số đo cảm biến đã bù, không chứng nhận nhiệt thực boiler. Không thay đổi setpoint/cutoff hoặc trình tự thủy lực trong lượt sửa này.

## Chạy test

Lệnh và trạng thái thực tế được cập nhật trong [test/README.md](../test/README.md) và [QA_STATUS.md](QA_STATUS.md). Exit 0 chỉ được dùng khi các assertions an toàn pass; không dùng kiểu reproducer “lỗi tái hiện = pass” cho gate mới.
