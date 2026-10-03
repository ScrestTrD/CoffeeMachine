# Logic Firmware v30

Đặc tả chi tiết luồng xử lý và logic điều khiển của CoffeeMachine Firmware v30.

## 1. Supervisor & Cơ chế an toàn nhiệt

Mỗi chu kỳ `tick()` thực hiện tuần tự:
1. `setBtn.poll()` và `runBtn.poll()`: Quét và giải mã sự kiện nút bấm.
2. `ntc.sample()`: Lấy mẫu ADC (9 mẫu × 1 ms), lọc trung vị và tính toán nhiệt độ.
3. `ntcTick()`: Giám sát an toàn NTC (loại trừ giai đoạn pha và cửa sổ hoãn 5s).
4. `pre-cut`: Ngắt SSR ngay trước FSM nếu không thỏa mãn quyền cấp nhiệt (`heatingPermission()`), trừ khi đang ở trong chu trình pha hoặc cửa sổ hoãn nhiệt 5s sau pha.
5. `FSM switch(state)`: Thực thi trạng thái hiện hành.
6. `applyHeating()`: Cập nhật điều khiển SSR theo mode hoạt động.
7. `applyLeds()`: Cập nhật trạng thái hiển thị của đèn LED SET và RUN.

### Các chế độ điều khiển nhiệt (`applyHeating`):
- **Chu trình pha (`brewingState`)**: Bao gồm `RUN_PUMP_PREDELAY`, `RUN_ACTIVE`, `PRESET_RECORD_ACTIVE`, `CLEAN_FLUSH`.
  - Trong pha ngâm ủ (Soak 2s) của preamble: SSR được chủ động ngắt OFF (bơm và van cũng OFF).
  - Trong các pha còn lại (Wet 2s, Press 2s, Chiết): SSR bị ép ON liên tục, bỏ qua độ trễ/lỗi tức thời của NTC để bù nhiệt nhanh do dòng nước cấp. Cầu chì nhiệt phần cứng là chốt bảo vệ độc lập.
  - Thiết lập cửa sổ hoãn lỗi NTC và duy trì nhiệt: `brewHeatUntilMs = now + 5000 ms`.
- **Trần an toàn phần mềm (`HEAT_CAP_C = 120°C`)**:
  - Áp dụng khi **không** ở trong trạng thái pha (`!brewingState(state)`).
  - Nếu nhiệt độ đo vượt ngưỡng 120°C, SSR lập tức bị ngắt. Đây là giới hạn bảo vệ tự động phục hồi khi nhiệt độ hạ xuống, không phải lỗi chốt cứng (latched fault).
- **Hồi nhiệt sau pha (`recoveringHeat_`)**:
  - Kích hoạt ngay khi kết thúc hoặc hủy chu trình pha.
  - Mục tiêu nhiệt độ được nâng lên mốc boost: `boostSp = setpoint + POST_BREW_BOOST_C` (mặc định setpoint + 10°C).
  - Điều khiển SSR ON khi nhiệt độ bù trễ `leadTemp()` < `boostSp`.
  - Khi nhiệt độ đạt dải `[boostSp, boostSp + 0.5°C]` và giữ ổn định trong 5 giây (`RECOVERY_STABLE_MS = 5000`, độ dao động toàn cửa sổ ≤ 0.10°C và không giảm quá 0.05°C từ đỉnh), chế độ hồi nhiệt kết thúc (`recoveringHeat_ = false`) và chuyển giao quyền điều khiển lại cho thermostat setpoint.
  - Lúc chuyển giao, nếu nhiệt độ thực tế vẫn thấp hơn setpoint, thermostat được kích hoạt khởi động nhiệt ngay (`requestHeat()`) để tránh vùng mù trễ hysteresis.
- **Thermostat thông thường**:
  - Đóng SSR khi `leadTemp() < setpoint - 0.5°C`.
  - Ngắt SSR khi `leadTemp() >= setpoint + 0.5°C`.

## 2. Bù trễ nhiệt đạo hàm (`leadT`) & Debounce NTC

- **Công thức bù trễ**:
  `leadT = rawT + NTC_LEAD_S * rate`
  Trong đó `rate = dT/dt` (°C/giây) được tính toán trên cửa sổ thời gian cố định 500 ms (`NTC_RATE_WINDOW_MS`), lọc thông thấp và kẹp giới hạn `|rate| <= 5.0 °C/s` để tránh nhiễu lượng tử hóa ADC.
- **Debounce lỗi NTC**:
  Một mẫu ADC xấu đơn lẻ (thường do nhiễu EMI khi đóng ngắt tải SSR/bơm) không làm máy báo lỗi ngay. Biến đếm `badCount_` yêu cầu phải có 5 cửa sổ lỗi liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) thì hệ thống mới chốt lỗi E1 (ngoài dải) hoặc E3 (timeout).
- **Cold Start**:
  Khi khởi động lạnh, hệ thống yêu cầu 2 cửa sổ đo liên tiếp đồng thuận chênh lệch không quá `NTC_MAX_STEP_C = 15°C` mới chấp nhận giá trị mồi bộ lọc, ngăn ngừa dữ liệu ADC rác ban đầu kích hoạt đun quá nhiệt.

## 3. Máy trạng thái (FSM) và Thời gian chu trình

1. **BOOT_SAFE**: Hiển thị số phiên bản firmware `30`. Chờ kết quả kiểm tra sensor NTC hợp lệ.
2. **STARTUP_PRIME**: Bơm chạy 5 giây, van đóng để mồi nước kín. Hiển thị `8888` nháy 2 Hz.
3. **HEATING_IDLE**: Đun nóng ban đầu. Nếu nhiệt độ < 80°C (`BLINK_BELOW_C`), hiển thị `8888` nháy; từ 80°C trở lên hiển thị `0000` đun tĩnh.
4. **READY_IDLE**: Đạt ngưỡng sẵn sàng (`controlTemp >= setpoint - 2°C`). Hiển thị `0000`, đèn RUN sáng tĩnh.
5. **SETPOINT_EDIT**: Giữ đồng thời SET+RUN trong 5 giây để vào. RUN tăng +1°C, SET giảm −1°C (dải 90–140°C). Giữ cả hai nút 300 ms để lưu flash; bỏ qua 20 giây không thao tác sẽ hủy bỏ.
6. **CLEAN_FLUSH**: Nhấn nhả nhanh nút SET (≤ 700 ms) từ trạng thái idle. Bơm ON, Van ON xả nước. Dừng bằng cách nhấn nhả SET trong khoảng 100 ms – < 3 s, hoặc tự ngắt sau 60 giây.
7. **RUN_PUMP_PREDELAY & RUN_ACTIVE**:
   - Preamble: Wet 2s (Bơm+Van+SSR ON) → Soak 2s (Bơm+Van+SSR OFF) → Press 2s (Bơm+SSR ON, Van OFF).
   - Chiết (RUN_ACTIVE): Mở van, bơm chạy, SSR chạy. Đếm ngược thời gian chiết theo preset đã lưu (tối đa 60 giây).
   - Hủy/Dừng khẩn cấp: Nhấn SET một lần hoặc giữ RUN 2 giây tại bất kỳ thời điểm nào của chu trình.
8. **FAULT_LATCHED**: Xảy ra lỗi an toàn (E1, E3, E6, E8). Ngắt toàn bộ actuator và hiển thị mã lỗi tương ứng cho đến khi reset nguồn.

## 4. Lưu trữ cấu hình (EEPROM v5)

- Struct `PersistentConfig` được bảo vệ bằng CRC32 và kiểm tra ngữ nghĩa dữ liệu.
- Định dạng v5 lưu trữ: `magic = 0x434F4545UL` ("COEE"), `version = 5`, setpoint nhiệt độ, tham số NTC (`R0 = 185000`, `Beta = 4890`), chỉ số preset gần nhất, và mảng 2 preset thời gian chiết.
- Áp dụng cấu hình và cập nhật trạng thái SSR trước khi thực hiện ghi flash đồng bộ để tránh trễ thời gian thực.
