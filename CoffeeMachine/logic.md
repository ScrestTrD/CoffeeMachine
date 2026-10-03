# Logic firmware v15

Đặc tả hành vi sửa; trạng thái kiểm chứng: [QA_STATUS](../docs/QA_STATUS.md).

## Supervisor

Mỗi tick: poll nút → sample NTC → supervisor/pre-cut nếu không được phép → FSM → áp yêu cầu nhiệt theo mode qua permission → LED. Permission dùng allowlist state vận hành; BOOT_SAFE/FAULT_LATCHED/state lạ luôn false. NTC phải hữu hạn, hợp lệ, mới dưới 2s; không fault. State lạ latch E8.

**v22/v23:** trong `brewingState`, `applyHeating()` ép D7 ON trước cửa permission (NTC lag/fault không ngắt được) và arm cửa sổ chống latch fault 5s. Sau pha, `recoveringHeat_` đun theo **nhiệt raw** lên **`set + POST_BREW_BOOST_C` (10°C)**; giữ ổn định tại mốc boost **5s** (`RECOVERY_STABLE_MS`, biên ≤0.10°C) rồi `recoveringHeat_=false` handoff về thermostat setpoint (`±0.5`). Seed `requestHeat()` nếu `raw < set` lúc handoff. Boost **không** chạy lúc boot. Cầu chì nhiệt là chốt an toàn phần cứng.

Sensor lỗi runtime latch E1 ngay lần publish; không publish hợp lệ trong 2s latch E3. Lỗi storage/save latch E6. Latch tắt bơm/van/SSR tới reset, không bị lệnh chốt SSR đảo ngược.

Có ba mode yêu cầu nhiệt: chu trình pha (RUN_PUMP_PREDELAY/RUN_ACTIVE/PRESET_RECORD_ACTIVE/CLEAN_FLUSH), hồi nhiệt sau pha, thermostat bình thường. Chu trình ép ON kể cả soak. Rời chu trình thật → arm hồi nhiệt: đun theo `raw` lên `set+10`, giữ ổn định 5s (biên ≤0.10°C, không tụt >0.05°C từ đỉnh) rồi handoff. Thermostat thường ON dưới `set−0.5`, OFF từ `set+0.5`. New cycle/fault xoá recovery; chọn/hủy trước pha không arm. Prime dùng pump-force/thermostat.
Cuối preset/record, chuyển mode và chốt SSR trước EEPROM commit. Vòng lặp không ghi mức ON cũ trước khi xử lý stop; pre-cut không an toàn vẫn chạy trước FSM. Không có cắt cứng mềm (v17): bảo vệ quá nhiệt là cầu chì nhiệt/thermostat phần cứng. Ngưỡng số đo không thay thermal fuse hoặc kết quả hiệu chuẩn thực.

## FSM và timer

Prime 5s van đóng; wet 2s van+bơm, soak 2s bơm/van OFF nhưng SSR vẫn yêu cầu ON, press 2s bơm/van đóng, rồi chiết. Display seconds từ RUN; preset seconds từ mở van chiết. Recording dùng flag “chiết đã bắt đầu”, không dùng timestamp 0 làm sentinel. Timeout 60s hủy recording trước khi xử lý RUN lưu ở cùng tick.

SET hoặc RUN hold 2s hủy ở RUN_PUMP_PREDELAY/RUN_ACTIVE. Sau mọi return/cancel/no-preset, readiness tính lại theo sensor. READY demote khi nguội; HEATING lên READY rồi return, tránh xử lý gesture hai lần.

CLEAN_FLUSH và SETPOINT_EDIT giữ gesture hiện có. Guard SET-held khi vào idle tránh dừng → release → tự xả. Timer unsigned subtraction chịu wrap; code test phải chạy qua mốc 0.

## Persistence

CRC/header bảo vệ cấu trúc; semantic validation bảo vệ miền giá trị. Setpoint load snap 0.5; NaN/inf/out-of-range và calibration/preset lỗi trả defaults. Preset ghi chỉ 1–60s. Config v5 hợp lệ giữ R0/Beta custom; thay defaults không âm thầm migrate calibration.

Save trả kết quả commit; failure latch E6 trước khi báo hoàn tất. Không commit khi setpoint/preset/lastPreset không đổi. EEPROM mock kiểm tra side-effect counts và reboot, không mô phỏng mất điện trong flash erase thực.

## Verification

Findings CM-001…007 và gates G01…10 được map tại [QA_GATES](../docs/QA_GATES.md). [Audit v6](../CODE_REVIEW_2026-09-30.md) là snapshot lỗi trước sửa; không phải trạng thái source mới.


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.
