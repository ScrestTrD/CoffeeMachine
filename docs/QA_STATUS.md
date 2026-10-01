# CoffeeMachine — QA v15

Baseline v14 commit75e184c, nhánh fix/brew-heat-recovery. V15 được duyệt để commit/push lên origin/main. Chưa nạp ESP; G10 NOT RUN.

## Thay đổi
1. Giám sát sau pha không handoff chỉ vì nằm trong [set,set+0.5°C] 3s. Toàn cửa sổ còn phải có range ≤0.10°C và giảm từ peak ≤0.05°C. Vượt mức thì khởi động lại3s. Tolerance phần mềm cần bench; trôi nhỏ vẫn có thể được xem là ổn định.
2. Đổi setpoint/clear latch/apply SSR trước synchronous EEPROM commit; failure vẫn latchesE6.
3. Giữ hai nút lưu cập nhật activity, không bị timeout20s hủy khi save đang tiến hành.
4. HEATING↔READY chỉ đổi state/hiển thị, giữ gesturetimers/queues. Stop/save chu trình vẫn dùng enterIdle để chống release gây xả ngoài ý muốn.

Trong pha vẫn đun qua soak theo permission. Giám sát: filtered<set ON, ≥set OFF ngay; không timed boost. Sau handoff thermostat±0.5°C. NTC+15°C/EEPROMv5/cutoff145°C không đổi.

## Kiểm chứng
| Gate | Kết quả |
|---|---|
| G01–G08 + H01–H28 | PASS114/114 normal và ASan/UBSan, compiler warning-as-error |
| Cùng114case trên sourcev14 | EXPECTED FAIL7case mỗi mode, xác nhận regression sensitivity |
| G09 | PASS ESP8266core3.1.2, ShiftRegister74HC5951.3.1; ISR trong IRAM |
| G10 | NOT RUN, không upload hoặc đo thiết bị |

Source SHA256 host/target: 4ee3a98d21a08bb31d9a94fcc322a0c35dd8201e39c600f244638c317ac3533d
Report hiện hành: host-gate-results.json, v14-hidden-reproduction.json, target-build-results.json và build-artifacts.json.
Các reports trước monitor/hidden-fix là lịch sử, không thay kết quả hiện hành.
Chi tiết sửa: [V15_HIDDEN_FIXES](V15_HIDDEN_FIXES_2026-10-01.md).

## Giới hạn
.10/.05°C là tiêu chí phần mềm, không chứng nhận NTC đáp ứng hoặc noise thật. Vẫn có trễ ADC/filter/vị trí đo/quán tính1400W và exact-set switching khi số đo nhiễu. Chưa thêm dwell/boost hoặc thay protection.
Quá nhiệt>145°C có thể cắt riêngSSR trong khi bơm tiếp tục; fault latches đến reset.
