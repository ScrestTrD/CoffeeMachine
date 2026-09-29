# G10 — bench test plan v7

Trạng thái: NOT RUN. Tài liệu chỉ quy định ca test và bằng chứng cần ghi; không điều khiển hoặc nạp thiết bị.

## Chuẩn bị bằng chứng

Ghi ngày/operator, board, sensor/SSR/relay, sơ đồ thực, firmware binary SHA256 được nạp, source SHA256 và toolchain. Không dùng số 7 lúc boot làm bằng chứng hash binary. Ghi số đo, trace, ảnh wiring và kết quả từng ca; đối chiếu với G01–G09 đã có.

## Các ca

| Case | Điều kiện / thao tác | Kỳ vọng | Bằng chứng |
|---|---|---|---|
| B01 | MCU unpowered/reset/upload, tải tách | Relay/SSR giữ OFF; straps GPIO0/2 HIGH, GPIO15 LOW | Đo chân điều khiển khi reset và cấp/rút nguồn |
| B02 | Display/nút/LED bằng low-voltage | 7 boot; prime/heat 8888 nháy; READY 0000; gesture/edit/cancel đúng; không tự xả sau stop | Video/logic trace |
| B03 | NTC open/short, ADC near rails | E1, tất cả actuator OFF và latch đến reset | Input/output trace và timing thực |
| B04 | Đo NTC đa điểm và so thermometer tin cậy | Xác lập R0/Beta/offset và sai số; không mặc định -18 đúng toàn dải | Bảng ADC/R/temp reference; lưu sensor placement |
| B05 | Prime/preamble/dose/record/clean dùng nước, heater tách | Prime 5s van đóng; wet/soak/press 2s; stop SET/RUN; record 60s hủy không ghi đè; không dead-head ngoài thiết kế | Timing/áp suất/lưu lượng và preset trước/sau |
| B06 | SSR input tại 3.3V, thermal fuse/thermostat độc lập đã kiểm tra | Trigger tin cậy, nguồn không sụt khi pump+valve+SSR ON; wiring/isolation đúng | Datasheet, volt/current, heatsink/fuse log |
| B07 | Heater với giám sát nhiệt/fixture phù hợp | Thermostat/cutoff phản ứng theo nhiệt raw/control; đo overshoot lúc bơm ép heat và hồi phục; ngưỡng phù hợp boiler/fuse | Trace reference temp/heater/pump, quyết định thermal limits |
| B08 | Preset/setpoint saves và reset/power interruption có kiểm soát | Ghi xác nhận sống qua restart; record hỏng CRC về defaults, không chạy preset rác; đánh giá mất record khi erase/write gián đoạn | Flash/preset trước-sau, thời điểm cắt nguồn |
| B09 | Nút/ADC/flow ISR khi pump/relay gây nhiễu | Không reset/treo/spurious dose; sensor fault không bị UI clear | Repeated-cycle/error trace |
| B10 | Full repeated cycles trên cả 2 preset và clean/edit | Giới hạn thời gian, abort/readiness/calibration đúng; không đếm xung để quyết định liều | Cycle log + các issues còn mở |

G10 chỉ PASS khi các ca có bằng chứng thực và chủ máy xác nhận phần electrical/thermal/hydraulic theo thiết kế. Host assertions không thay những phép đo này. Ngưỡng 145°C và trình tự van đóng lúc prime/press đang giữ theo quyết định source cũ, chưa được lượt này chứng nhận cho cấu hình máy thực.

G10 chưa chạy vì PC không kết nối ESP, theo xác nhận của chủ dự án ngày 2026-09-30. Không có thao tác upload hoặc điều khiển thiết bị trong lượt commit/push.
