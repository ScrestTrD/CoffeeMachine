
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

### docs(boiler): document dual-boiler temperature decoupling
- **Mô tả**: Nguyên lý độc lập nhiệt giữa nồi hơi pha cà phê và nồi hơi tạo bọt sữa.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(grinder): add optional serial grinder synchronization note
- **Mô tả**: Ghi chú giao thức đồng bộ cối xay cà phê qua cổng nối tiếp tương lai.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(qa): document end-to-end hardware-in-the-loop tests
- **Mô tả**: Mô tả quy trình kiểm thử tự động HIL với board mạch giả lập cảm biến.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(telemetry): add error code diagnostic lookup table
- **Mô tả**: Bảng tra cứu mã lỗi E1-E5 cho kỹ thuật viên vận hành bảo dưỡng.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(steam): document steam wand thermal cutoff protection
- **Mô tả**: Bảo vệ ngắt nhiệt thanh gia nhiệt vòi sục sữa khi cạn nước.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(pump): note ulka vibration pump duty cycle restrictions
- **Mô tả**: Giới hạn chu kỳ làm việc liên tục của bơm rung Ulka chống quá nhiệt cuộn dây.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(spec): summarize electrical power budget specifications
- **Mô tả**: Tổng kết công suất tiêu thụ tối đa của toàn bộ linh kiện hệ thống máy.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

### docs(arch): clarify NTC lead compensation formula
- **Mô tả**: Giải thích công thức đạo hàm leadT = rawT + NTC_LEAD_S * rate nhằm bù trễ nhiệt nắp nồi.
- **Cập nhật**: Vòng lặp cải tiến tài liệu firmware v30.

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
