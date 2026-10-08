
### docs(arch): clarify NTC lead compensation formula
- **Mô tả**: Giải thích công thức đạo hàm leadT = rawT + NTC_LEAD_S * rate nhằm bù trễ nhiệt nắp nồi.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(safety): document hardware thermal fuse specifications
- **Mô tả**: Bổ sung thông số cầu chì nhiệt phần cứng dự phòng cấp 2 cho SSR.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(flow): describe 74HC595 shift register pinout layout
- **Mô tả**: Ghi chú sơ đồ chân kết nối IC ghi dịch 74HC595 điều khiển cụm van và bơm.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(firmware): document pre-infusion soak timing constraints
- **Mô tả**: Chi tiết về khoảng thời gian soak 2s trong chu trình ngâm ủ cà phê.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(recovery): clarify post-brew boost temperature recovery
- **Mô tả**: Mô tả cơ chế đẩy nhiệt thêm 10 độ C sau chiết xuất và thời gian ổn định 5s.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(ops): note emergency shutdown hotkey sequence
- **Mô tả**: Ghi chú tổ hợp nút dừng khẩn cấp hệ thống đun và bơm khi có sự cố.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(eeprom): document memory allocation map for user presets
- **Mô tả**: Bản đồ phân vùng nhớ EEPROM lưu các cấu hình nhiệt độ và định lượng nước.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(ui): specify display refresh rate and debounce intervals
- **Mô tả**: Tần số quét màn hình LED 7 đoạn và thời gian debounce nút nhấn.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(sensor): document water level probe conductivity thresholds
- **Mô tả**: Ngưỡng đo điện dẫn que thăm mực nước bình chứa.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(thermostat): add PID tuning parameters reference table
- **Mô tả**: Bảng tham chiếu hệ số PID nhiệt độ nồi hơi cho các loại hạt rang khác nhau.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(wiring): clarify 220V AC isolation barrier clearance
- **Mô tả**: Quy định khoảng cách cách điện an toàn cho phần rơ-le bán dẫn SSR 220V.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(ota): add firmware flashing guidelines via nodemcu usb
- **Mô tả**: Hướng dẫn nạp firmware an toàn qua cổng USB CH340 của NodeMCU.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(maintenance): add weekly descale maintenance schedule
- **Mô tả**: Lịch trình bảo dưỡng tẩy cặn định kỳ bằng dung dịch acid citric.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(flowmeter): describe hall effect pulse counter scaling
- **Mô tả**: Tỷ lệ xung cảm biến lưu lượng dòng chảy tính trên mỗi ml nước chiết.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(pressure): document OPV valve threshold calibration
- **Mô tả**: Hiệu chuẩn van áp suất quá tải OPV bảo vệ đường ống chiết xuất.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(network): note future esp8266 telemetry websocket schema
- **Mô tả**: Phác thảo schema bản tin WebSocket đo từ xa gửi về dashboard local.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.
