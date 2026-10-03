# CoffeeMachine — hướng dẫn vận hành

Tài liệu theo source v15 [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine/CoffeeMachine.ino). Source và firmware đang chạy là hai trạng thái cần xác minh riêng. [QA_STATUS](docs/QA_STATUS.md) ghi bằng chứng đã có.

## Thao tác nhanh

| Việc | Thao tác |
|---|---|
| Pha | RUN chọn → SET đổi preset → RUN xác nhận |
| Dừng pha | SET press hoặc giữ RUN 2s; áp dụng cả preamble/chiết |
| Ghi | SET giữ 3s → RUN đổi slot → SET chốt → RUN bắt đầu → RUN dừng/lưu; SET hủy |
| Giới hạn ghi | 60s chiết tự dừng/hủy, không lưu; 0s không ghi đè |
| Xả | SET nhấn-nhả ≤700ms; dừng bằng SET nhấn-nhả 100ms–<3s; tự dừng 60s |
| Chỉnh nhiệt | SET+RUN 5s rồi nhả → RUN +1/SET −1°C → giữ cả hai 300ms lưu; timeout 20s hủy |

BOOT_SAFE hiện 15; prime/đun hiện 8888 nháy; READY hiện 0000/RUN steady và tính lại theo nhiệt. Dosing được phép từ heating idle, nên chờ READY nếu cần nhiệt ổn định.

## Đun trong pha và hồi nhiệt

Trong pha/ghi preset, máy yêu cầu đun liên tục qua cả wet/ngâm/nén/chiết; lỗi NTC vẫn ưu tiên. Sau dừng/kết thúc/hủy/timeout/xả, nếu NTC sau lọc dưới nhiệt cài đặt thì tiếp tục đun; nếu đã đạt/vượt thì tắt ngay. Sau pha giữ giám sát: filtered < set bật SSR ngay, filtered ≥ set tắt ngay. Chỉ trả về thermostat ±0,5°C sau các mẫu mới liên tiếp trong [set, set+0,5°C] đủ 3 giây. Ra khỏi vùng, đổi set hoặc bị bảo vệ ngắt thì tính lại; mẫu cũ không kéo dài xác nhận. Đây là thời gian quan sát, không ép đun thêm. Không ép đun thêm vài giây.

Ví dụ set97.5°C: kết thúc pha ở97.25°C thì đun tới≥97.5°C rồi tắt; kết thúc ở98°C thì tắt ngay. Trong giám sát, xuống97.2°C đã bật lại. Sau xác nhận ổn định và trở về thermostat thì dưới97°C mới bật. Độ trễ vật lý NTC vẫn cần đo. Công suất1400W có thể gây tăng nhiệt tiếp sau khi tắt; chưa có đo thực nghiệm mới trong lượt này.

## Mã và bảo vệ

E1 NTC invalid, E3 timeout mẫu 2s, E6 storage init/save thất bại, E8 FSM state lạ. Fault latch tắt SSR/bơm/van đến reset. no nghĩa là chưa có preset hợp lệ.

Không còn cắt cứng mềm theo nhiệt độ (v17): bảo vệ quá nhiệt là cầu chì nhiệt/thermostat phần cứng độc lập. Firmware chỉ thermostat quanh setpoint; NTC ngoài dải latch E1. Không coi calibration là chứng nhận nhiệt boiler.

## Hiệu chuẩn và service

- NTC defaults 185000Ω/Beta 4890/offset +15 (v11: fit 2 điểm + bù đọc thấp đều 15, 2026-09-30 22:39); config EEPROM v5 hợp lệ giữ R0/Beta riêng.
- [test/NtcTempMonitor/](test/NtcTempMonitor/) quan sát defaults/filter; không đại diện config EEPROM custom hoặc toàn bộ safety logic.
- [test/NTC_Temperature.txt](test/NTC_Temperature.txt) là sketch hiệu chuẩn tham chiếu dùng Serial, defaults 100k và không offset. Khi fit lại R0/Beta phải cập nhật cấu hình có chủ đích (đổi CFG_VERSION để ép defaults), không cộng bù hai lần.
- Sketch test không thay firmware production và không tự giữ tất cả actuator OFF. Tách tải trước service. GPIO TX/RX chia sẻ LED, không để UART bridge chọi tín hiệu khi vận hành.
- AP/HTTP/OTA chưa triển khai; Preview_AP.html chỉ mockup cũ.
- Wiring/fuse/SSR/NTC đa điểm/power-loss cần các [gate hardware](docs/QA_GATES.md); lượt này không nạp chip hoặc chạy máy.

[Hướng dẫn chi tiết](CoffeeMachine/instruction.md).


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.
