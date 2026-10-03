# CoffeeMachine — QA Status (Firmware v30)

Phiên bản hiện hành: **v30** (commit `8147e6e` trên `main`), cập nhật ngày 2026-10-03.

## Tóm tắt thay đổi từ v15 đến v30

1. **v16**: Bỏ bù trừ cố định `+15°C` do kết quả đo thực tế báo cao hơn nhiệt độ chuẩn.
2. **v17**: Bỏ cắt cứng phần mềm 145°C (`ABS_OVERTEMP_C`), chuyển hoàn toàn việc bảo vệ quá nhiệt tối cao sang cầu chì nhiệt phần cứng độc lập.
3. **v18**: Giữ SSR ép bật trong suốt chu trình pha và duy trì cửa sổ hoãn 5 giây (`BREW_HEAT_HOLD_MS = 5000`) sau khi kết thúc pha, tạm hoãn bắt lỗi NTC để tránh ngắt nhiệt do trễ cảm biến.
4. **v19**: Hiển thị `8888` nháy chỉ khi nồi lạnh (`< 80°C`); từ 80°C trở lên hiển thị `0000` đun tĩnh.
5. **v20**: Hồi nhiệt sau pha chuyển sang đọc theo nhiệt độ `rawTemp()` để phản ứng nhanh hơn độ trễ lọc thông thấp. Mồi `requestHeat()` khi handoff về thermostat nếu nhiệt độ còn dưới setpoint.
6. **v21**: Chống nhảy số bất thường ở NTC (`NTC_MAX_STEP_C = 15°C`) và yêu cầu 2 cửa sổ đo ban đầu đồng thuận khi cold start.
7. **v22 / v23**: Nâng mục tiêu hồi nhiệt sau pha lên `setpoint + 10°C` (`POST_BREW_BOOST_C = 10.0f`), duy trì ổn định trong 5 giây (`RECOVERY_STABLE_MS = 5000`) rồi mới bàn giao cho thermostat.
8. **v24 / v25**: Bổ sung trần an toàn phần mềm `HEAT_CAP_C = 120°C` tự động ngắt SSR khi không pha nếu nhiệt độ đạt 120°C.
9. **v26 / v28**: Bổ sung bù trễ nhiệt đạo hàm `leadT = rawT + NTC_LEAD_S * rate` (đo trên cửa sổ 500 ms, `NTC_LEAD_S = 10s`).
10. **v27**: Rút ngắn chu kỳ lấy mẫu NTC xuống ~9 ms (9 mẫu × 1 ms).
11. **v29**: Debounce lỗi NTC với 5 cửa sổ xấu liên tiếp (`NTC_FAULT_DEBOUNCE = 5`) trước khi latch lỗi E1/E3.
12. **v30**: Ngắt SSR trong 2 giây ngâm ủ (Soak) của chu trình preamble. Bổ sung bản build chẩn đoán `test/CoffeeMachineDebug`.

## Kiểm chứng và trạng thái Gates

| Gate | Mô tả | Trạng thái |
|---|---|---|
| G01–G08 | Host unit test & logic FSM | PASS trên commit v30 |
| G09 | Target build ESP8266 Core 3.1.2 + ShiftRegister74HC595 | PASS trên commit v30 |
| G10 | Kiểm chứng phần cứng vật lý trên máy thực tế | Đang thực hiện bench testing và nạp thử nghiệm |
