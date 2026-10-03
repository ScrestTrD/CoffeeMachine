# Hướng dẫn vận hành — source v15

Firmware trên máy chỉ thay đổi sau khi được nạp. Xem [QA_STATUS](../docs/QA_STATUS.md) trước khi xác nhận bản này đã nghiệm thu.

## Khởi động

BOOT_SAFE hiện 15 → sau mẫu NTC hợp lệ bơm chạy 5s, van đóng, 8888 và hai LED nháy 2Hz → đun (8888 nháy, RUN 1Hz) → sẵn sàng (0000, RUN steady). READY LED chuyển về trạng thái HEATING khi nhiệt lọc giảm dưới setpoint−2°C; đây không phải ngưỡng đóng/cắt SSR. Không giữ RUN khi reset vì GPIO15 phải LOW để boot.

## Pha preset

RUN vào chọn → SET đổi slot hợp lệ → RUN xác nhận. Máy ướt 2s, ngâm 2s, nén 2s rồi chiết. Màn hình đếm từ RUN; liều tính từ lúc mở van chiết. SET press hoặc RUN giữ 2s dừng ở bất kỳ pha preamble/chiết. Hết preset hoặc 60s chiết tự dừng. Không có preset hiện no, bấm để thoát.

## Ghi preset

SET giữ 3s → RUN chọn 1/2 → SET chốt → RUN bắt đầu. RUN lần nữa chốt số giây chiết; SET hủy. 0 giây không ghi đè. Hết 60s chiết tự dừng và hủy, không tự lưu. Lỗi lưu EEPROM E6 tắt mọi actuator; cần kiểm tra storage và ghi lại sau reset.

## Xả và chỉnh nhiệt

SET idle nhấn-nhả ≤0.7s vào xả. SET nhấn-nhả 100ms–<3s dừng; tự dừng 60s. RUN không điều khiển xả.

SET+RUN đủ 5s rồi nhả vào edit; RUN +1°C, SET −1°C trong 90–140°C. Giữ cả hai 300ms lưu; bỏ 20s không bấm hủy. Giá trị được snap 0.5°C; nếu không đổi thì không ghi EEPROM.

## Đun trong pha và hồi nhiệt

Trong pha/ghi preset, máy yêu cầu đun liên tục qua cả wet/ngâm/nén/chiết; **v18: D7 không ngắt trong pha kể cả NTC lag/fault, và giữ ON thêm 5 giây sau pha rồi mới kiểm nhiệt lại**. Sau dừng/kết thúc/hủy/timeout/xả, nếu NTC sau lọc dưới nhiệt cài đặt thì tiếp tục đun; nếu đã đạt/vượt thì tắt ngay. Sau pha giữ giám sát: filtered < set bật SSR ngay, filtered ≥ set tắt ngay. Chỉ trả về thermostat ±0,5°C sau các mẫu mới liên tiếp trong [set, set+0,5°C] đủ 3 giây. Ra khỏi vùng, đổi set hoặc bị bảo vệ ngắt thì tính lại; mẫu cũ không kéo dài xác nhận. Đây là thời gian quan sát, không ép đun thêm. Không ép đun thêm vài giây.

Ví dụ set97.5°C: kết thúc pha ở97.25°C thì đun tới≥97.5°C rồi tắt; kết thúc ở98°C thì tắt ngay. Trong giám sát, xuống97.2°C đã bật lại. Sau xác nhận ổn định và trở về thermostat thì dưới97°C mới bật. Độ trễ vật lý NTC vẫn cần đo. Công suất1400W có thể gây tăng nhiệt tiếp sau khi tắt; chưa có đo thực nghiệm mới trong lượt này.

## Lỗi và nhiệt

E1 NTC invalid; E3 timeout mẫu boot/runtime; E6 EEPROM begin/save; E8 FSM state lạ. Fault latch mọi actuator OFF tới reset. Kiểm tra nguyên nhân trước reset. **Không còn cắt cứng mềm theo nhiệt độ (v17)** — bảo vệ quá nhiệt bằng cầu chì nhiệt/thermostat phần cứng.

Chu trình pha yêu cầu gia nhiệt nhưng NTC/fault vẫn thắng. Thermal fuse/thermostat độc lập là bảo vệ quá nhiệt duy nhất; bench tests vẫn cần.


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.
