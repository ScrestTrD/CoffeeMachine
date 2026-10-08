# CoffeeMachine

Controller NodeMCU ESP8266 cho máy pha cà phê. Code chính thức: [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine/CoffeeMachine.ino). Code thử nghiệm và chẩn đoán nằm trong [test/](test/README.md).

## Phiên bản và trạng thái (Firmware v30)

Phiên bản hiện hành: **v30** (`FW_VERSION = 30`).
- **Trần an toàn phần mềm (`HEAT_CAP_C = 120°C`)**: Tự động ngắt SSR khi nhiệt độ đạt ngưỡng 120°C ở trạng thái không pha (bảo vệ chống trôi nhiệt cao do NTC đọc thiếu). Tự phục hồi khi nhiệt giảm. Cầu chì nhiệt phần cứng là chốt chặn cuối cùng.
- **Bù trễ nhiệt đạo hàm (`leadT`)**: `leadT = rawT + NTC_LEAD_S * rate` (cửa sổ 500 ms, `NTC_LEAD_S = 10s`) giúp thermostat và hồi nhiệt phản ứng sớm trước quán tính nhiệt của nồi.
- **Chu kỳ lấy mẫu NTC nhanh**: 9 mẫu × 1 ms (~9 ms/cửa sổ) giúp giảm độ trễ đo lường.
- **Debounce lỗi NTC**: Cần 5 cửa sổ lỗi liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) mới kích hoạt mã lỗi E1/E3, chống nhiễu đóng ngắt SSR.
- **Tắt SSR trong 2s soak**: Trong pha soak của chu trình ngâm ủ preamble (bơm và van đều tắt), SSR cũng được tắt; SSR bật lại ở pha nén và chiết.
- **Hồi nhiệt sau pha (`POST_BREW_BOOST_C = 10°C`)**: Sau khi kết thúc pha, hệ thống bù nhiệt đẩy lên `setpoint + 10°C`, giữ ổn định 5 giây (`RECOVERY_STABLE_MS = 5000`) theo `leadTemp()` rồi mới chuyển giao về thermostat bình thường.

Repository gateway: `/mnt/pc-dev/ScrestTrD/CoffeMachine`, qua share `//192.168.1.171/Develop`. PC .171: `H:\Develop\ScrestTrD\CoffeMachine`.

## Build và vận hành

Mở sketch `CoffeeMachine/CoffeeMachine.ino` trong Arduino IDE.
Dependencies:
- ESP8266 Arduino core (kèm thư viện EEPROM)
- `ShiftRegister74HC595` (Timo Denk)

Tài liệu chi tiết:
- [Hướng dẫn vận hành](HUONG_DAN_VAN_HANH.md)
- [Tổng quan](CoffeeMachine/readme.md), [hardware](CoffeeMachine/hardware.md), [logic](CoffeeMachine/logic.md), [thao tác chi tiết](CoffeeMachine/instruction.md)
- [Spec hiện hành](COFFE_README.md)
- [Gates và test acceptance](docs/QA_GATES.md)
- [QA Status](docs/QA_STATUS.md)

Lưu ý: `Preview_AP.html` là mockup lịch sử; source hiện hành không có WiFi AP, HTTP handler hoặc OTA. Dosing theo timer giây (không phụ thuộc xung flowmeter trong điều khiển FSM).

<!-- verified pair badge trigger -->

<!-- pair verify slnctrz author + scresttrd coauthor -->
