# CoffeeMachine — firmware v7

Source controller NodeMCU ESP8266: [CoffeeMachine.ino](CoffeeMachine.ino). Source đã sửa và pass host/target gates G01–G09; xem [QA_STATUS](../docs/QA_STATUS.md) cho trạng thái kiểm chứng.

## Chức năng

Thermostat ON/OFF 1°C hysteresis; bơm ép yêu cầu gia nhiệt qua safety supervisor. NTC median 7 mẫu/6ms, Beta conversion (R0 185000/Beta 4890, fit 2 điểm NTC mới), low-pass alpha 0.25. SSR cutoff xét cả nhiệt chưa lọc và nhiệt đã lọc >145°C. NTC/fault/storage/state lạ luôn thắng UI.

2 preset giây chiết, preamble 2+2+2s; 60s giới hạn pha chiết và ghi preset. Recording timeout hủy, không ghi đè. SET và RUN hold 2s dừng pha từ preamble. Xả vệ sinh có timeout 60s. Chỉnh setpoint 90–140°C bằng nút, không WiFi/PWM.

READY được tính lại mỗi tick; HEATING hiện 8888 nháy, READY hiện 0000. FW_VERSION hiện lúc BOOT_SAFE; version không thay hash binary.

## Kiến trúc

| Khối | Vai trò |
|---|---|
| Display | Frame 4 digit, active-LOW, chỉ shift khi đổi |
| Button | Debounce 25ms + queue pressed/released/held |
| NtcSensor | Mẫu hợp lệ, tuổi mẫu, raw đã bù và nhiệt lọc |
| FlowSensor | ISR diagnostic; không quyết định dosing |
| Persistence | EEPROM v5 + CRC/semantic validation + trạng thái save |
| Thermostat | ON/OFF, dead-band 1°C |
| CoffeeMachine | FSM, safety supervisor, UI và actuator |

EEPROM v5 hợp lệ giữ calibration/preset; không tự reset calibration khi defaults source đổi — ngoại lệ một lần v9: lên version 5 để ép defaults mới (R0 185000/Beta 4890) vì giá trị cũ đã biết sai, preset phải ghi lại. Config invalid về defaults; EEPROM init/commit failure latch E6. E1 NTC invalid, E3 timeout, E8 state lạ. Overtemp không latch, chỉ cắt SSR.

[Hardware](hardware.md), [logic](logic.md), [instruction](instruction.md), [spec](../COFFE_README.md), [gates](../docs/QA_GATES.md), [audit baseline v6](../CODE_REVIEW_2026-09-30.md).
