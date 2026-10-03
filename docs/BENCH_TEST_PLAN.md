# G10 — bench test plan (Firmware v30)

Trạng thái: NOT RUN. Tài liệu chỉ quy định ca test và bằng chứng cần ghi; không điều khiển hoặc nạp thiết bị.

## Chuẩn bị bằng chứng

Ghi ngày/operator, board, sensor/SSR/relay, sơ đồ thực, firmware binary SHA256 được nạp, source SHA256 và toolchain. Không dùng số 30 lúc boot làm bằng chứng hash binary. Ghi số đo, trace, ảnh wiring và kết quả từng ca; đối chiếu với G01–G09 đã có.

## Các ca

| Case | Điều kiện / thao tác | Kỳ vọng | Bằng chứng |
|---|---|---|---|
| B01 | MCU unpowered/reset/upload, tải tách | Relay/SSR giữ OFF; straps GPIO0/2 HIGH, GPIO15 LOW | Đo chân điều khiển khi reset và cấp/rút nguồn |
| B02 | Display/nút/LED bằng low-voltage | 30 boot; prime/heat 8888 nháy; READY 0000; gesture/edit/cancel đúng; không tự xả sau stop | Video/logic trace |
| B03 | NTC open/short, ADC near rails | E1, tất cả actuator OFF và latch đến reset (debounce 5 cửa sổ) | Input/output trace và timing thực |
| B04 | Đo NTC đa điểm và so thermometer tin cậy | Xác lập R0/Beta/offset và sai số; kiểm tra lead compensation | Bảng ADC/R/temp reference; lưu sensor placement |
| B05 | Prime/preamble/dose/record/clean dùng nước, heater tách | Prime 5s van đóng; wet/soak/press 2s (soak SSR OFF); stop SET/RUN; record 60s hủy không ghi đè | Timing/áp suất/lưu lượng và preset trước/sau |
| B06 | SSR input tại 3.3V, thermal fuse/thermostat độc lập đã kiểm tra | Trigger tin cậy, nguồn không sụt khi pump+valve+SSR ON; wiring/isolation đúng | Datasheet, volt/current, heatsink/fuse log |
| B07 | Heater với giám sát nhiệt/fixture phù hợp | Thermostat/heat-cap 120°C phản ứng theo nhiệt lead/raw; đo overshoot lúc hồi nhiệt post-brew boost +10°C | Trace reference temp/heater/pump |
| B08 | Preset/setpoint saves và reset/power interruption có kiểm soát | Ghi xác nhận sống qua restart; record hỏng CRC về defaults, không chạy preset rác | Flash/preset trước-sau, thời điểm cắt nguồn |
| B09 | Nút/ADC/flow ISR khi pump/relay gây nhiễu | Không reset/treo/spurious dose; debounce NTC 5 cửa sổ chống lỗi gián đoạn | Repeated-cycle/error trace |
| B10 | Full repeated cycles trên cả 2 preset và clean/edit | Giới hạn thời gian, abort/readiness/calibration đúng | Cycle log + các issues còn mở |
| B11 | Trace preset và recording gồm wet/soak/press/extraction | D7 HIGH suốt chu trình trừ 2s soak của preamble; trần 120°C áp dụng ngoài pha | Đồng bộ D7/pump/valve/leadT/setpoint |
| B12 | Hồi nhiệt sau pha (post-brew boost) | Sau pha đun lên setpoint+10°C, giữ ổn định 5s rồi handoff về thermostat; không bị kẹt SSR | Trace tại stop, sample publish và handoff |
| B13 | Theo dõi bộ 1400W qua nhiều chu kỳ sau khi SSR OFF | Ghi overshoot, thời gian hồi, độ trễ sensor | Reference thermometer + vị trí sensor + dòng heater |
