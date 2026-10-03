# Hướng dẫn thao tác và vận hành — Firmware v30

Tài liệu chi tiết hướng dẫn thao tác máy pha cà phê sử dụng Firmware v30 [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine.ino).

## 1. Khởi động và trạng thái ban đầu

1. **Khởi động nguồn (`BOOT_SAFE`)**:
   - Màn hình 7 đoạn 4 số hiển thị số phiên bản firmware: **30**.
   - Hệ thống kiểm tra tính hợp lệ của cảm biến NTC (yêu cầu 2 cửa sổ đo ban đầu đồng thuận) và bộ nhớ EEPROM.
2. **Mồi nước khởi động (`STARTUP_PRIME`)**:
   - Bơm chạy trong 5 giây, van xả đóng (chạy tuần hoàn kín không xả nước ra ngoài).
   - Màn hình hiển thị `8888` nháy cùng 2 đèn LED với tần số 2 Hz.
3. **Gia nhiệt khởi động (`HEATING_IDLE`)**:
   - Khi nhiệt độ dưới 80°C (`BLINK_BELOW_C`), màn hình nháy `8888`.
   - Khi đạt từ 80°C trở lên, màn hình chuyển sang hiển thị `0000` tĩnh và tiếp tục đun cho đến khi đạt setpoint.
   - Đèn LED RUN nháy chậm 1 Hz.
4. **Sẵn sàng pha (`READY_IDLE`)**:
   - Khi nhiệt độ thực tế đạt ngưỡng `setpoint - 2°C`, máy chuyển sang trạng thái sẵn sàng.
   - Màn hình hiển thị `0000` tĩnh, đèn LED RUN sáng liên tục.

*Lưu ý an toàn*: Không nhấn giữ nút RUN trong lúc bật nguồn hoặc reset, vì chân RUN kết nối với GPIO15 (yêu cầu kéo LOW lúc boot ESP8266).

## 2. Pha cà phê theo Preset

1. **Chọn và xác nhận**:
   - Từ trạng thái Idle, nhấn nút **RUN** để vào menu chọn preset. Màn hình hiển thị chỉ số preset (ví dụ `   1` hoặc `   2`). Nếu chưa có preset nào được lưu, màn hình hiện `  no`.
   - Nhấn nút **SET** để chuyển đổi qua lại giữa các preset khả dụng.
   - Nhấn nút **RUN** để bắt đầu chu trình pha.
2. **Diễn tiến chu trình**:
   - **Giai đoạn ngâm ủ (Preamble)**:
     - *Wet (2s)*: Van mở, bơm chạy, SSR bật (làm ướt bột cà phê).
     - *Soak (2s)*: Van đóng, bơm ngắt, SSR ngắt (ngâm ủ không ép nhiệt).
     - *Press (2s)*: Van đóng, bơm chạy, SSR bật (tạo áp suất).
   - **Giai đoạn chiết xuất (Extraction)**: Van mở, bơm chạy, SSR bật. Màn hình đếm giây chiết xuất.
   - Khi hết thời gian chiết của preset (hoặc chạm giới hạn bảo vệ 60 giây), van và bơm tự động ngắt.
3. **Dừng pha khẩn cấp**:
   - Nhấn nhanh nút **SET** một lần, hoặc nhấn giữ nút **RUN** trong 2 giây tại bất kỳ thời điểm nào của chu trình để dừng pha ngay lập tức.

## 3. Ghi nhớ Preset mới (Học thời gian chiết)

1. Từ trạng thái Idle, nhấn và **giữ nút SET trong 3 giây**.
2. Màn hình nhấp nháy chỉ số preset (`   1` hoặc `   2`). Nhấn **RUN** để đổi slot cần ghi, nhấn **SET** để chọn slot.
3. Đèn LED SET nháy nhanh, màn hình sáng tĩnh slot đã chọn. Nhấn **RUN** để bắt đầu chu trình chạy ghi nhớ (chạy qua đúng các pha ngâm ủ như khi pha thật).
4. Khi lượng cà phê chiết xuất đã đạt yêu cầu:
   - Nhấn nút **RUN** một lần để chốt dừng và lưu thời gian chiết xuất thực tế (không tính thời gian ngâm ủ preamble).
   - Hoặc nhấn nút **SET** để hủy bỏ việc lưu mà không làm mất thời gian cũ.
   - Nếu thời gian chiết vượt quá 60 giây, máy sẽ tự ngắt và hủy bỏ ghi nhớ.

## 4. Xả nước vệ sinh (Clean Flush)

- Từ trạng thái Idle, nhấn và nhả nhanh nút **SET** (thời gian giữ ≤ 700 ms).
- Van và bơm cùng bật để xả nước vệ sinh đầu họng pha. Màn hình đếm số giây xả.
- Dừng xả: Nhấn và nhả nhanh nút **SET** một lần nữa (thời gian giữ từ 100 ms đến dưới 3 giây), hoặc máy sẽ tự động ngắt sau 60 giây.

## 5. Chỉnh nhiệt độ cài đặt (Setpoint Edit)

1. Nhấn và **giữ đồng thời cả hai nút SET + RUN trong 5 giây**, sau đó nhả ra.
2. Màn hình nhấp nháy giá trị nhiệt độ cài đặt hiện tại (ví dụ ` 93.0`).
3. Điều chỉnh:
   - Nhấn **RUN** để tăng +1.0°C.
   - Nhấn **SET** để giảm −1.0°C.
   - Dải nhiệt độ cho phép điều chỉnh: từ 90.0°C đến 140.0°C.
4. Lưu và thoát:
   - Nhấn và **giữ đồng thời cả hai nút SET + RUN trong 300 ms** để lưu giá trị mới vào flash EEPROM.
   - Nếu không thao tác trong vòng 20 giây, hệ thống sẽ tự động thoát ra ngoài và hủy bỏ các thay đổi.

## 6. Cơ chế hồi nhiệt sau pha & Trần an toàn (v30)

- **Hồi nhiệt sau pha**: Sau khi pha hoặc xả nước xong, máy tự động đẩy nhiệt độ lên mốc `setpoint + 10°C` theo nhiệt độ bù trễ đạo hàm `leadTemp()`, duy trì ổn định trong 5 giây rồi mới trả về thermostat kiểm soát thông thường.
- **Trần bảo vệ phần mềm 120°C**: Khi không pha, nếu nhiệt độ đo đạt ngưỡng 120°C, SSR tự động bị ngắt để tránh tình trạng trôi nhiệt quá cao do sai số NTC vùng nhiệt độ cao.
- **Mã lỗi an toàn (E1, E3, E6, E8)**: Khi có lỗi cảm biến hoặc bộ nhớ, mọi tải đóng ngắt đều bị ngắt và khóa an toàn. Cần kiểm tra phần cứng và tắt/bật lại nguồn để reset máy.
