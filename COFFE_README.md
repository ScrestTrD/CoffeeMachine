# CoffeeMachine — Đặc tả kỹ thuật hiện hành (Firmware v30)

Cập nhật 2026-10-03 theo Firmware v30 [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine/CoffeeMachine.ino). Trạng thái kiểm chứng nằm trong [QA_STATUS](docs/QA_STATUS.md).

## 1. Phần cứng (Hardware)

NodeMCU ESP8266 (ESP-12E); [pin map và wiring](CoffeeMachine/hardware.md) là tài liệu tham chiếu chi tiết:
- D6/GPIO12: Bơm (active-HIGH).
- D5/GPIO14: Van điện từ (active-HIGH).
- D7/GPIO13: SSR thanh nhiệt (active-HIGH), logic 3.3V tĩnh, không PWM.
- D0/GPIO16: Nút SET (active-LOW, điện trở pull-up ngoài 10k).
- D8/GPIO15: Nút RUN (active-HIGH, điện trở pull-down ngoài 10k, bắt buộc LOW khi boot).
- D2/D3/D4: Mạch hiển thị 4x 74HC595 (SDI, SCLK, LOAD).
- RX (GPIO3) và TX (GPIO1): Đèn LED SET và RUN (active-HIGH qua điện trở 1k). Bản production không gọi `Serial.begin()`.
- A0: ADC đọc cảm biến NTC 100k (cầu phân áp với R_SERIES 10k lên 3.3V).
- D1/GPIO5: Tín hiệu flowmeter (ngắt FALLING, chỉ dùng cho chẩn đoán).

## 2. Kiểm soát nhiệt độ (Thermostat & An toàn v30)

- **Thermostat**: Bật SSR khi `leadTemp < setpoint − 0.5°C`, tắt SSR khi `leadTemp >= setpoint + 0.5°C` (dead-band 1.0°C).
- **Dải nhiệt độ**: Mặc định 97.5°C; cho phép cài đặt 90.0–140.0°C (bước nhảy snap 0.5°C).
- **Cảm biến NTC**: Mặc định `R0 = 185000 Ω`, `Beta = 4890 K`, `offset = 0°C`.
  - Cửa sổ lấy mẫu: 9 mẫu ADC cách nhau 1 ms (~9 ms/cửa sổ).
  - Lọc thông thấp kết hợp tính bù trễ đạo hàm: `leadT = rawT + NTC_LEAD_S * rate` (cửa sổ 500 ms, `NTC_LEAD_S = 10s`).
  - Debounce lỗi NTC: Yêu cầu 5 cửa sổ xấu liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) trước khi báo lỗi E1/E3.
  - Khởi động lạnh yêu cầu 2 cửa sổ đo liên tiếp đồng thuận chênh lệch ≤ 15°C mới nhận dữ liệu.
- **Trần an toàn phần mềm (`HEAT_CAP_C = 120°C`)**:
  - Khi không pha, nếu nhiệt độ đo đạt 120°C, SSR tự động bị ngắt nhằm chống trôi nhiệt cao do sai số NTC. Tự phục hồi khi nhiệt độ hạ xuống.
  - Cầu chì nhiệt vật lý độc lập là lớp bảo vệ quá nhiệt tối cao.
- **Chu trình pha & ngâm ủ**:
  - Wet (2s): Bơm ON, Van ON, SSR ON.
  - Soak (2s): Bơm OFF, Van OFF, SSR OFF (v30 ngắt SSR trong giai đoạn ngâm ủ).
  - Press (2s): Bơm ON, Van OFF, SSR ON.
  - Chiết: Bơm ON, Van ON, SSR ON.
  - Duy trì ép bật SSR trong các pha pha chiết và hoãn bắt lỗi NTC 5 giây sau pha (`BREW_HEAT_HOLD_MS = 5000`).
- **Hồi nhiệt sau pha**: Đẩy nhiệt độ lên `setpoint + 10°C` (`POST_BREW_BOOST_C = 10.0f`), duy trì ổn định 5 giây (`RECOVERY_STABLE_MS = 5000`) theo `leadTemp()` rồi mới chuyển giao về thermostat.

## 3. Trình tự hoạt động và Giao diện (UI)

| Trạng thái | Hành vi chi tiết |
|---|---|
| `BOOT_SAFE` | Tất cả actuator OFF, màn hình hiển thị số phiên bản `30`. Chờ khởi tạo sensor và bộ nhớ |
| `STARTUP_PRIME` | 5 giây bơm chạy mồi nước tuần hoàn (van đóng); hiển thị `8888` nháy cùng 2 LED 2 Hz |
| `HEATING_IDLE` | Gia nhiệt khởi động: nháy `8888` khi < 80°C; hiển thị `0000` tĩnh khi ≥ 80°C. Đèn RUN nháy 1 Hz |
| `READY_IDLE` | Nhiệt độ đạt ngưỡng sẵn sàng (`>= setpoint - 2°C`). Hiển thị `0000`, đèn RUN sáng liên tục |
| Chọn pha | Nhấn RUN từ idle, dùng SET đổi preset, RUN xác nhận để bắt đầu; nếu chưa có preset hiện `  no` |
| Preamble | Wet 2s (Bơm+Van+SSR ON) → Soak 2s (Bơm+Van+SSR OFF) → Press 2s (Bơm+SSR ON, Van OFF) |
| `RUN_ACTIVE` | Van mở, bơm chạy, SSR chạy. Đếm thời gian chiết xuất thực tế (tối đa 60 giây) |
| Dừng pha | Nhấn SET một lần hoặc giữ RUN 2 giây tại bất kỳ thời điểm nào của chu trình |
| Ghi preset | Giữ SET 3s → RUN chọn slot → SET chốt → RUN bắt đầu → RUN lưu / SET hủy |
| `CLEAN_FLUSH` | SET nhấn-nhả ≤ 700 ms; bơm và van cùng mở xả nước. Dừng bằng cách nhấn SET hoặc tự ngắt sau 60 giây |
| `SETPOINT_EDIT` | Giữ SET+RUN trong 5 giây; RUN tăng +1°C, SET giảm −1°C; giữ cả hai 300 ms để lưu |

## 4. Cấu hình EEPROM (v5)

- Sử dụng cấu trúc `PersistentConfig` với CRC32, `CFG_MAGIC = 0x434F4545UL` ("COEE"), `CFG_VERSION = 5`.
- Lưu trữ setpoint nhiệt độ, tham số NTC R0/Beta, và 2 slot preset thời gian chiết (1–60 giây).
- Dữ liệu hỏng hoặc không hợp lệ sẽ tự động nạp giá trị mặc định an toàn. Lỗi khởi tạo hoặc ghi lưu EEPROM sẽ kích hoạt mã lỗi E6 và ngắt toàn bộ actuator.

## 5. Bảng mã lỗi (Fault Codes)

| Mã | Ý nghĩa | Xử lý |
|---|---|---|
| **E1** | Cảm biến NTC hở/chập/ngoài dải cho phép (đã qua debounce 5 cửa sổ) | Latch lỗi, ngắt toàn bộ tải |
| **E3** | Quá thời gian chờ mẫu NTC mới (timeout 2 giây) | Latch lỗi, ngắt toàn bộ tải |
| **E6** | Lỗi khởi tạo hoặc ghi lưu EEPROM thất bại | Latch lỗi, ngắt toàn bộ tải |
| **E8** | Trạng thái máy FSM không hợp lệ | Latch lỗi, ngắt toàn bộ tải |
| **no** | Chưa có preset hợp lệ được cài đặt | Bấm phím bất kỳ để thoát về idle |
