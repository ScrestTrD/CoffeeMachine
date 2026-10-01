# CoffeeMachine

Controller NodeMCU ESP8266 cho máy pha cà phê. Code chính thức: [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine/CoffeeMachine.ino). Code thử nghiệm nằm trong [test/](test/README.md).

## Phiên bản và trạng thái

Source v14 triển khai điều khiển nhiệt đã duyệt trên baseline v11: đun suốt pha kể cả soak, giám sát sau pha và chỉ trả về thermostat khi số đo ổn định 3 giây. Host105/105 và G09 pass; G10 hardware chưa chạy. Firmware trên thiết bị chưa được nạp lại trong lượt này. Xem [QA_STATUS](docs/QA_STATUS.md) để biết gate nào đã chạy; số version lúc boot không chứng minh binary trùng source.

Repository gateway: /mnt/pc-dev/CoffeMachine, qua share //192.168.1.171/Develop. Đã xác minh trên PC .171: H:\Develop\CoffeMachine là repository tương ứng.

## Build và vận hành

Mở sketch CoffeeMachine/CoffeeMachine.ino trong Arduino IDE. Dependencies: ESP8266 Arduino core (EEPROM đi kèm) và ShiftRegister74HC595 của Timo Denk. Đã target-build với ESP8266 core 3.1.2 và ShiftRegister74HC595 1.3.1; metadata CLI/compiler/library trong target-build-results.json. Kết quả này không thay G10.

- [Hướng dẫn vận hành](HUONG_DAN_VAN_HANH.md)
- [Tổng quan](CoffeeMachine/readme.md), [hardware](CoffeeMachine/hardware.md), [logic](CoffeeMachine/logic.md), [thao tác chi tiết](CoffeeMachine/instruction.md)
- [Spec hiện hành](COFFE_README.md)
- [Gates và test acceptance](docs/QA_GATES.md)
- [Rà soát từng khối và quyết định nhiệt v14](docs/HEAT_CONTROL_REVIEW_2026-10-01.md)
- [Audit baseline v6](CODE_REVIEW_2026-09-30.md)

Preview_AP.html là mockup lịch sử; source hiện hành không có WiFi AP, HTTP handler hoặc OTA. PID/PWM và dosing theo xung không thuộc implementation này.
