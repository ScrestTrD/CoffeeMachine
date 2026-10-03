# CoffeeMachine — Firmware v30

Source controller NodeMCU ESP8266: [CoffeeMachine.ino](CoffeeMachine.ino). Code thử nghiệm và chẩn đoán nằm trong [test/](../test/README.md).

## Chức năng chính (v30)

- **Thermostat điều nhiệt**: Cơ chế ON/OFF với hysteresis 1°C quanh setpoint (`setpoint ± 0.5°C`).
- **Bù trễ nhiệt đạo hàm (`leadT`)**: Tính toán theo công thức `leadT = rawT + NTC_LEAD_S * rate` (đo trên cửa sổ 500 ms, hằng số thời gian `NTC_LEAD_S = 10s`). Giúp phát hiện sớm xu hướng tăng/giảm nhiệt để cắt và đóng SSR kịp thời.
- **Trần an toàn phần mềm (`HEAT_CAP_C = 120°C`)**: Khi không ở trong trạng thái pha, SSR tự động bị cắt nếu nhiệt độ chạm ngưỡng 120°C (phòng ngừa trôi nhiệt cao do đặc tính NTC đọc thiếu). Tự động phục hồi khi nhiệt độ hạ xuống dưới ngưỡng.
- **Chu trình ngâm ủ & pha chiết (Preamble)**:
  - Wet (2s): Van ON, Bơm ON, SSR ON.
  - Soak (2s): Van OFF, Bơm OFF, SSR OFF (v30 ngắt nhiệt trong giai đoạn ngâm ủ).
  - Press (2s): Van OFF, Bơm ON, SSR ON.
  - Extraction: Van ON, Bơm ON, SSR ON liên tục.
- **Hồi nhiệt sau pha (Post-Brew Recovery)**: Sau khi kết thúc pha, hệ thống bù nhiệt đẩy lên `setpoint + 10°C` (`POST_BREW_BOOST_C = 10.0f`), duy trì ổn định trong 5 giây (`RECOVERY_STABLE_MS = 5000`) theo nhiệt độ `leadTemp()` rồi mới chuyển giao về thermostat thông thường.
- **Lấy mẫu & Debounce NTC**: 9 mẫu × 1 ms (~9 ms/cửa sổ); yêu cầu 5 cửa sổ xấu liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) mới kích hoạt lỗi E1/E3 để tránh nhiễu do đóng ngắt SSR.
- **Giao diện & Hiển thị**: Led 7 đoạn 4 số qua 4 IC 74HC595 (quét lại khung hình tĩnh mỗi 5 ms). Hiển thị số phiên bản `30` khi khởi động (`BOOT_SAFE`).
- **Dosing**: 2 preset thời gian chiết (tính bằng giây từ lúc mở van chiết, giới hạn tối đa 60 giây).

## Kiến trúc hệ thống

| Khối | Vai trò |
|---|---|
| `Display` | Quản lý khung hình 4 digit, giải mã 7-segment active-HIGH (common-cathode), refresh liên tục mỗi 5 ms chống nhiễu latch |
| `Button` | Debounce 25 ms, hàng đợi sự kiện (pressed / released / held) cho nút SET và RUN |
| `NtcSensor` | Lấy mẫu ADC, chuyển đổi điện trở NTC, tính toán `rawTemp`, `controlTemp` (lọc thông thấp), và `leadTemp` (bù trễ đạo hàm); debounce lỗi |
| `FlowSensor` | Đếm xung ngắt IRAM phục vụ chẩn đoán (không can thiệp quyết định liều pha trong FSM) |
| `Persistence` | Lưu trữ EEPROM v5, kiểm tra toàn vẹn CRC32, setpoint và 2 preset giây chiết |
| `Thermostat` | Điều khiển đóng/cắt SSR theo setpoint với dead-band 1°C |
| `CoffeeMachine` | FSM điều khiển luồng hoạt động, giám sát an toàn (fail-closed), đồng bộ actuator và đèn LED |

Tài liệu liên quan:
- [Hardware](hardware.md)
- [Logic](logic.md)
- [Instruction](instruction.md)
- [Spec COFFE_README](../COFFE_README.md)
- [QA Status](../docs/QA_STATUS.md)
