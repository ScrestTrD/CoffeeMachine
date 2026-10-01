# CoffeeMachine — QA v14, giám sát sau pha

Cập nhật 2026-10-01. Nhánh fix/brew-heat-recovery, baseline v11 commit1720787. Bản v14 được duyệt để commit/push lên origin/main. Chưa nạp ESP; trạng thái G10 vẫn NOT RUN.

## Hành vi hiện hành
- Suốt pha/record/clean, kể cả soak bơm OFF: yêu cầu đun; bảo vệ ưu tiên.
- Sau mọi kết thúc/hủy/timeout chu trình thật: giữ giám sát. Filtered <set -> SSR ON; ≥set -> OFF ngay. Chạm set không xóa giám sát.
- Chỉ trả về thermostat thường khi các mẫu mới liên tiếp nằm trong [set, set+0.5°C] ít nhất 3000ms. Đây là tiêu chí phần mềm, chưa phải ổn định nhiệt thực.
- Ra ngoài vùng, thay setpoint, bảo vệ chặn heat hoặc khoảng publish ≥2s: tính lại xác nhận. Mẫu lặp không tiến timer. Dùng phép trừ unsigned qua rollover.
- Sau handoff, thermostat v11 ON dưới set−0.5°C, OFF từ set+0.5°C. Không bảo đảm hết mọi trễ NTC; nếu không đạt điều kiện ổn định thì tiếp tục giám sát, không có timeout ép đun.
- Prime/cancel trước pha không arm. Pha mới/fault xóa trạng thái giám sát cũ.
- NTC +15°C, EEPROM v5, cutoff145°C và các mã E1/E3/E6/E8 giữ nguyên.

## Kết quả
| Kiểm tra | Trạng thái |
|---|---|
| G01–G08, H01–H19 | PASS 105/105 normal và ASan/UBSan; compile warning-as-error |
| Cùng105 case trên snapshot trước sửa | EXPECTED FAIL 6 case ở mỗi mode: H03/H05/H16/H17/H18/H19 |
| Audit bổ sung A01–A06 | PASS 6 nhóm; gồm grid56 trường hợp pha và56 trường hợp kết thúc |
| G09 ESP8266 | PASS, core3.1.2, ShiftRegister74HC5951.3.1, ISR trong IRAM |
| G10 phần cứng | NOT RUN; chưa nạp ESP |

Source SHA256 host/target: 934afe590319bb7dd719405fc71697a37a9b4525ccc20e382882576b756f44f4

Bằng chứng: host-gate-results.json, pre-monitor-reproduction.json, heat-monitor-audit-results.json, target-build-results.json, build-artifacts.json.
Báo cáo v11-heat-reproduction.json (101 case/26 fail), heat-deep-audit-results.json và hai review ngày2026-10-01 là lịch sử trước thay đổi này.

## Giới hạn
Thử bước ADC từ filtered103 xuống raw calibrated95.9722: bật lại sau252ms/840ms/4200ms ở nhịp tick giả lập6/20/100ms. Không phải đo nhịp ESP hoặc nhiệt nước thực; bộ lọc và trễ sensor vẫn tồn tại.
Bảo vệ145°C vẫn có thể cắt riêng SSR khi bơm chạy. Fault vẫn khóa đến reset.
RAM28668/80192, IRAM60051/65536 (gồm cache32768), flash241720/1048576. Build có cảnh báo môi trường HOME và SyntaxWarning của toolchain; không lỗi compile/link.
