# CoffeeMachine — Hướng dẫn vận hành (Firmware v30)

Tài liệu chuẩn theo source firmware v30 [CoffeeMachine/CoffeeMachine.ino](CoffeeMachine/CoffeeMachine.ino).

## Thao tác nhanh

| Việc | Thao tác |
|---|---|
| Pha | RUN chọn → SET đổi preset → RUN xác nhận |
| Dừng pha | SET press hoặc giữ RUN 2s (hỗ trợ cả trong preamble lẫn chiết) |
| Ghi | SET giữ 3s → RUN đổi slot → SET chốt → RUN bắt đầu → RUN dừng/lưu; SET hủy |
| Giới hạn ghi | 60s chiết tự dừng/hủy, không lưu; 0s không ghi đè |
| Xả vệ sinh | SET nhấn-nhả ≤700ms; dừng bằng SET nhấn-nhả 100ms–<3s; tự dừng sau 60s |
| Chỉnh nhiệt | SET+RUN giữ 5s rồi nhả → RUN +1°C / SET −1°C → giữ cả hai nút 300ms để lưu; timeout 20s hủy |

- Khi khởi động, màn hình `BOOT_SAFE` hiển thị số phiên bản: **30**.
- Khởi động prime / đun ban đầu: hiển thị `8888` nháy khi nhiệt độ dưới 80°C (`BLINK_BELOW_C`), trên 80°C hiển thị `0000` đun tĩnh.
- Trạng thái sẵn sàng (`READY`): hiển thị `0000`, đèn `RUN LED` sáng liên tục (steady ON).

## Chu trình đun và hồi nhiệt (v30)

1. **Trong chu trình pha (Preamble & Chiết)**:
   - Pha 1 (Wet 2s): Bơm ON, Van ON, SSR ON.
   - Pha 2 (Soak 2s): Bơm OFF, Van OFF, SSR OFF (ngắt đun để ngâm ủ tự nhiên).
   - Pha 3 (Press 2s): Bơm ON, Van OFF, SSR ON (nén áp suất).
   - Pha 4 (Chiết): Bơm ON, Van ON, SSR ON liên tục.
   - Trong suốt các pha có đun, SSR được ép ON bất chấp độ trễ NTC nhằm bù tụt nhiệt khi nước lạnh cấp vào nồi. Cầu chì nhiệt phần cứng là chốt bảo vệ độc lập.

2. **Hồi nhiệt sau pha (`POST_BREW_BOOST_C = 10°C`)**:
   - Sau khi kết thúc hoặc dừng pha, hệ thống tự động kích hoạt chế độ hồi nhiệt: nâng nhiệt độ nồi lên mốc `setpoint + 10°C`.
   - Điều khiển theo nhiệt độ bù trễ đạo hàm `leadTemp()` (`leadT = rawT + NTC_LEAD_S * rate`).
   - Duy trì ổn định tại ngưỡng boost trong 5 giây (`RECOVERY_STABLE_MS = 5000`) trước khi bàn giao lại cho thermostat thông thường (`setpoint ± 0.5°C`).

3. **Trần an toàn phần mềm (`HEAT_CAP_C = 120°C`)**:
   - Ở các trạng thái không pha, nếu nhiệt độ đo đạt ngưỡng 120°C, SSR tự động ngắt để bảo vệ quá nhiệt (chống trôi nhiệt cao do NTC đọc thiếu). Hệ thống tự phục hồi đun khi nhiệt độ hạ xuống dưới ngưỡng.

## Mã lỗi và bảo vệ

- **E1**: Lỗi cảm biến NTC ngoài dải cho phép (đã qua debounce 5 cửa sổ xấu liên tiếp).
- **E3**: Timeout đọc mẫu NTC (quá 2 giây không có mẫu mới).
- **E6**: Lỗi khởi tạo hoặc ghi lưu bộ nhớ EEPROM.
- **E8**: Trạng thái FSM không hợp lệ.
- **"no"**: Chưa có preset hợp lệ được cài đặt.

Khi xảy ra lỗi (E1/E3/E6/E8), toàn bộ actuator (bơm, van, SSR) lập tức ngắt hoàn toàn (fail-closed) và khóa cứng cho đến khi khởi động lại máy.

## Cấu hình cảm biến và lưu trữ

- Cảm biến NTC: Mặc định `R0 = 185000 Ω`, `Beta = 4890`, `offset = 0°C`. Cửa sổ lấy mẫu nhanh 9 mẫu × 1 ms (~9 ms).
- Bộ nhớ EEPROM: Chuẩn `CFG_VERSION = 5`, lưu trữ setpoint nhiệt độ và 2 slot preset thời gian chiết (giây).
