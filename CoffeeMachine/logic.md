# Logic firmware v7

Đặc tả hành vi sửa; trạng thái kiểm chứng: [QA_STATUS](../docs/QA_STATUS.md).

## Supervisor

Mỗi tick: poll nút → sample NTC → supervisor → thermostat → SSR qua permission → FSM → chốt SSR qua permission → LED. Permission dùng allowlist state vận hành; BOOT_SAFE/FAULT_LATCHED/state lạ luôn false. NTC phải hữu hạn, hợp lệ, mới dưới 2s; không fault; cả raw đã bù và control ≤145°C. State lạ latch E8.

Sensor lỗi runtime latch E1 ngay lần publish; không publish hợp lệ trong 2s latch E3. Lỗi storage/save latch E6. Latch tắt bơm/van/SSR tới reset, không bị lệnh chốt SSR đảo ngược.

SSR yêu cầu ON nếu thermostat ON hoặc pump ON; overtemp chỉ khóa SSR, tự hồi khi cả raw/control ≤145. Ngưỡng số đo không thay thermal fuse hoặc kết quả hiệu chuẩn thực.

## FSM và timer

Prime 5s van đóng; wet 2s van+bơm, soak 2s cả hai OFF, press 2s bơm/van đóng, rồi chiết. Display seconds từ RUN; preset seconds từ mở van chiết. Recording dùng flag “chiết đã bắt đầu”, không dùng timestamp 0 làm sentinel. Timeout 60s hủy recording trước khi xử lý RUN lưu ở cùng tick.

SET hoặc RUN hold 2s hủy ở RUN_PUMP_PREDELAY/RUN_ACTIVE. Sau mọi return/cancel/no-preset, readiness tính lại theo sensor. READY demote khi nguội; HEATING lên READY rồi return, tránh xử lý gesture hai lần.

CLEAN_FLUSH và SETPOINT_EDIT giữ gesture hiện có. Guard SET-held khi vào idle tránh dừng → release → tự xả. Timer unsigned subtraction chịu wrap; code test phải chạy qua mốc 0.

## Persistence

CRC/header bảo vệ cấu trúc; semantic validation bảo vệ miền giá trị. Setpoint load snap 0.5; NaN/inf/out-of-range và calibration/preset lỗi trả defaults. Preset ghi chỉ 1–60s. Config v3 hợp lệ giữ R0/Beta custom; thay defaults không âm thầm migrate calibration.

Save trả kết quả commit; failure latch E6 trước khi báo hoàn tất. Không commit khi setpoint/preset/lastPreset không đổi. EEPROM mock kiểm tra side-effect counts và reboot, không mô phỏng mất điện trong flash erase thực.

## Verification

Findings CM-001…007 và gates G01…10 được map tại [QA_GATES](../docs/QA_GATES.md). [Audit v6](../CODE_REVIEW_2026-09-30.md) là snapshot lỗi trước sửa; không phải trạng thái source mới.
