# Hướng dẫn vận hành — source v7

Firmware trên máy chỉ thay đổi sau khi được nạp. Xem [QA_STATUS](../docs/QA_STATUS.md) trước khi xác nhận bản này đã nghiệm thu.

## Khởi động

BOOT_SAFE hiện 7 → sau mẫu NTC hợp lệ bơm chạy 5s, van đóng, 8888 và hai LED nháy 2Hz → đun (8888 nháy, RUN 1Hz) → sẵn sàng (0000, RUN steady). READY trở lại đun khi nhiệt giảm dưới setpoint−2°C. Không giữ RUN khi reset vì GPIO15 phải LOW để boot.

## Pha preset

RUN vào chọn → SET đổi slot hợp lệ → RUN xác nhận. Máy ướt 2s, ngâm 2s, nén 2s rồi chiết. Màn hình đếm từ RUN; liều tính từ lúc mở van chiết. SET press hoặc RUN giữ 2s dừng ở bất kỳ pha preamble/chiết. Hết preset hoặc 60s chiết tự dừng. Không có preset hiện no, bấm để thoát.

## Ghi preset

SET giữ 3s → RUN chọn 1/2 → SET chốt → RUN bắt đầu. RUN lần nữa chốt số giây chiết; SET hủy. 0 giây không ghi đè. Hết 60s chiết tự dừng và hủy, không tự lưu. Lỗi lưu EEPROM E6 tắt mọi actuator; cần kiểm tra storage và ghi lại sau reset.

## Xả và chỉnh nhiệt

SET idle nhấn-nhả ≤0.7s vào xả. SET nhấn-nhả 100ms–<3s dừng; tự dừng 60s. RUN không điều khiển xả.

SET+RUN đủ 5s rồi nhả vào edit; RUN +1°C, SET −1°C trong 90–140°C. Giữ cả hai 300ms lưu; bỏ 20s không bấm hủy. Giá trị được snap 0.5°C; nếu không đổi thì không ghi EEPROM.

## Lỗi và nhiệt

E1 NTC invalid; E3 timeout mẫu boot/runtime; E6 EEPROM begin/save; E8 FSM state lạ. Fault latch mọi actuator OFF tới reset. Kiểm tra nguyên nhân trước reset. Quá 145°C ở nhiệt chưa lọc đã bù hoặc nhiệt lọc chỉ cắt SSR, không latch và không cắt bơm/van.

Bơm ON ép yêu cầu gia nhiệt nhưng NTC/fault/cutoff vẫn thắng. Thermal fuse/thermostat độc lập và bench tests vẫn cần; source v7 chưa tự chứng minh heater/hydraulic an toàn.
