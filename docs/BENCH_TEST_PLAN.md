# G10 — bench test plan v15

Trạng thái: NOT RUN. Tài liệu chỉ quy định ca test và bằng chứng cần ghi; không điều khiển hoặc nạp thiết bị.

## Chuẩn bị bằng chứng

Ghi ngày/operator, board, sensor/SSR/relay, sơ đồ thực, firmware binary SHA256 được nạp, source SHA256 và toolchain. Không dùng số 15 lúc boot làm bằng chứng hash binary. Ghi số đo, trace, ảnh wiring và kết quả từng ca; đối chiếu với G01–G09 đã có.

## Các ca

| Case | Điều kiện / thao tác | Kỳ vọng | Bằng chứng |
|---|---|---|---|
| B01 | MCU unpowered/reset/upload, tải tách | Relay/SSR giữ OFF; straps GPIO0/2 HIGH, GPIO15 LOW | Đo chân điều khiển khi reset và cấp/rút nguồn |
| B02 | Display/nút/LED bằng low-voltage | 14 boot; prime/heat 8888 nháy; READY 0000; gesture/edit/cancel đúng; không tự xả sau stop | Video/logic trace |
| B03 | NTC open/short, ADC near rails | E1, tất cả actuator OFF và latch đến reset | Input/output trace và timing thực |
| B04 | Đo NTC đa điểm và so thermometer tin cậy | Xác lập R0/Beta/offset và sai số; không mặc định offset+15 đúng toàn dải | Bảng ADC/R/temp reference; lưu sensor placement |
| B05 | Prime/preamble/dose/record/clean dùng nước, heater tách | Prime 5s van đóng; wet/soak/press 2s; stop SET/RUN; record 60s hủy không ghi đè; không dead-head ngoài thiết kế | Timing/áp suất/lưu lượng và preset trước/sau |
| B06 | SSR input tại 3.3V, thermal fuse/thermostat độc lập đã kiểm tra | Trigger tin cậy, nguồn không sụt khi pump+valve+SSR ON; wiring/isolation đúng | Datasheet, volt/current, heatsink/fuse log |
| B07 | Heater với giám sát nhiệt/fixture phù hợp | Thermostat/cutoff phản ứng theo nhiệt raw/control; đo overshoot lúc bơm ép heat và hồi phục; ngưỡng phù hợp boiler/fuse | Trace reference temp/heater/pump, quyết định thermal limits |
| B08 | Preset/setpoint saves và reset/power interruption có kiểm soát | Ghi xác nhận sống qua restart; record hỏng CRC về defaults, không chạy preset rác; đánh giá mất record khi erase/write gián đoạn | Flash/preset trước-sau, thời điểm cắt nguồn |
| B09 | Nút/ADC/flow ISR khi pump/relay gây nhiễu | Không reset/treo/spurious dose; sensor fault không bị UI clear | Repeated-cycle/error trace |
| B10 | Full repeated cycles trên cả 2 preset và clean/edit | Giới hạn thời gian, abort/readiness/calibration đúng; không đếm xung để quyết định liều | Cycle log + các issues còn mở |
| B11 | Trace preset và recording gồm wet/soak/press/extraction | D7 HIGH suốt chu trình khi permission hợp lệ, kể cả pump OFF lúc soak; cutoff vẫn thắng | Đồng bộ D7/pump/valve/raw/control/setpoint |
| B12 | Kết thúc/hủy/stop/timeout/clean với nhiệt lọc dưới/bằng/trên set | Dưới set thìON; đã≥set thìOFF ngay; giảm lại dưới set thìON ngay trước ổn định3s; không ON sai do latch cũ hoặc commit flash | Trace tại stop, EEPROM save và sample publish |
| B13 | Theo dõi bộ1400W qua nhiều chu kỳ sau khi SSR OFF | Ghi overshoot, thời gian hồi, độ trễ sensor; không suy ra từ host mocks | Reference thermometer + vị trí sensor + dòng heater |

G10 chỉ PASS khi các ca có bằng chứng thực và chủ máy xác nhận phần electrical/thermal/hydraulic theo thiết kế. Host assertions không thay những phép đo này. Ngưỡng 145°C và trình tự van đóng lúc prime/press đang giữ theo quyết định source cũ, chưa được lượt này chứng nhận cho cấu hình máy thực.

G10 chưa chạy vì PC không kết nối ESP, theo xác nhận của chủ dự án ngày 2026-09-30. Không có thao tác upload hoặc điều khiển thiết bị trong lượt commit/push.


V15: xác nhận 3 giây còn yêu cầu biên độ toàn cửa sổ ≤0,10°C và giảm từ đỉnh ≤0,05°C; vượt ngưỡng thì tính lại. Đây là dung sai phần mềm chưa kiểm chứng nhiễu NTC thực. SSR vẫn bật dưới set/tắt từ set; không ép đun theo timer. Giảm chậm hơn dung sai vẫn có thể được coi là ổn định; không dự đoán nhiệt tương lai.

Setpoint mới và SSR được áp dụng trước ghi flash; giữ hai nút lưu được tính là hoạt động để không bị timeout20s hủy. Chuyển HEATING↔READY giữ timer/nút; guard chống release sau STOP vẫn giữ ở các đường kết thúc chu trình.

## Bổ sung v15
- B14: nhiệt sau pha đang giảm xuyên vùng set..set+0.5; giám sát không thoát nếu range>0.10 hoặc giảm từ peak>0.05. Xác minh noise/tần suất handoff thực.
- B15: hạ set trong khiSSR đangON; đoD7 trước/trong commit; mô phỏng savefail trên fixture.
- B16: SET record, SET+RUN edit, SET shortclean đi qua cảhai readiness transitions; thao tác vẫn hoàn tất, release sauSTOP không mởxả.
