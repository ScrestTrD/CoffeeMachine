> Lịch sử trước bản sửa giám sát sau pha. Hành vi hiện hành và bằng chứng mới: [QA_STATUS](QA_STATUS.md).

# Deep audit nhiệt — 2026-10-01

## Phạm vi và kết luận
Audit bản v14 đang sửa trên fix/brew-heat-recovery, chưa commit/push; không phải xác nhận firmware đang nạp trên ESP.
Source SHA256: a83c5dd2c2e39e42ae5f174561ace1bdd6a1a00141237ed96a9ab21d57d545ad.
Đã rà các khối FSM, đường ghi SSR, thermostat, NTC/ADC, bảo vệ, lưu cấu hình, nút và các đường kết thúc/hủy pha. Không sửa production trong lượt audit.

Triệu chứng mất nhiệt sâu/chậm hồi vẫn CÓ THỂ xảy ra. Không tìm thấy đường tắt SSR chỉ vì bơm dừng trong chu trình pha hợp lệ ở v14. Không thể suy ra nhiệt thực từ lệnh D7 hoặc chứng nhận mọi tình huống vật lý bằng host test.

## F1 — Khoảng chờ sau pha vẫn tồn tại theo chính sách đã duyệt
CoffeeMachine/CoffeeMachine.ino: applyHeating (khoảng 900–923), Thermostat::update (764–770).
Kết thúc pha, filtered >= set làm tắt SSR và xóa recoveringHeat_. Thermostat đã clear nên chỉ bật lại khi filtered < set - 0.5, không phải ngay khi filtered < set.
Ví dụ set 97.5: filtered 103/raw 90 khi kết thúc -> OFF; filtered 97.25/raw 85 -> vẫn OFF; đúng 97.0 -> vẫn OFF; 96.99 -> ON.
Đây là phản ví dụ logic, các nhiệt độ được đưa vào fixture để kiểm tra điều kiện; không khẳng định máy thực có độ lệch raw/filter như vậy.
Nếu NTC đang báo nóng hơn vùng nước/khối đang nguội, mất nhiệt thực có thể tiếp diễn trong khoảng OFF. Đây là giới hạn chính sách đã duyệt, không phải lỗi mới đi ngược yêu cầu.
Một lần chạm set trong hồi nhiệt cũng xóa recovery; nếu số đo lại giảm, vẫn phải qua deadband như trên (A04). Không có xác nhận ổn định/xu hướng.

## F2 — Độ trễ bộ lọc phụ thuộc nhịp loop; timeout không đo chất lượng đáp ứng
NtcSensor::sample 431–439: mỗi lần gọi chỉ lấy tối đa một mẫu; 7 mẫu mới publish. publishMedian 492–500: EMA alpha 0.25.
Test dùng đường ADC/conversion/filter thực trong firmware, đầu filtered 103, ADC bước xuống mức calibrated 95.9722:
| Nhịp gọi tick giả lập | Thời gian đến SSR ON | Filter khi ON |
|---|---:|---:|
| 6 ms | 294 ms | 96.9103 |
| 20 ms | 980 ms | 96.9103 |
| 100 ms | 4900 ms | 96.9103 |
Ở nhịp 100 ms, publish mỗi 700 ms nên vẫn không E3 dù đáp ứng chậm. Đây là kịch bản nhịp giả lập, KHÔNG chứng minh ESP thực đang chạy chậm.
Với loop nhanh, riêng bộ lọc không đủ bằng chứng giải thích trễ nhiều giây. Trễ tiếp xúc NTC, vị trí đo và quán tính khối gia nhiệt chưa đo.
Lưu flash đồng bộ có thể kéo dài một tick; không có số đo để quy kết là nguyên nhân thường xuyên.

## F3 — Bảo vệ 145°C có thể tắt riêng SSR trong khi vẫn bơm
heatingPermission 988–1002 kiểm tra CẢ raw calibrated và filtered <=145. Một trong hai >145 -> SSR OFF, không tự dừng bơm ở nhánh này. Khi cả hai trở lại hợp lệ -> được đun lại.
A05 tái hiện raw 145.1/filtered 90 và trường hợp ngược lại: D7 LOW, bơm HIGH.
Đây là hành vi kế thừa v11, ưu tiên bảo vệ. Raw ở đây là giá trị từ median đã +15°C, không phải ADC tức thời.
Không được kết luận cutoff là nguyên nhân nếu chưa có trace vượt ngưỡng. Không đề nghị bỏ bảo vệ.

## F4 — Fault và cấu hình có thể tạo biểu hiện tương tự
E1 NTC invalid, E3 timeout, E6 lưu trữ, E8 state lỗi khóa toàn bộ actuator đến reset; không tự hồi dù NTC sau đó bình thường. Phân biệt với cutoff 145°C chỉ tắt nhiệt.
Giữ nguyên mã lỗi v11; không có thay đổi E1 thành chỉ >180°C trong v14.
EEPROM hợp lệ giữ calibration/setpoint cũ. CRC/range hợp lệ không chứng minh calibration đúng phần cứng. Offset +15°C cũng không phải phép đo nhiệt nước. Sai số có thể làm điều khiển tin rằng đã đủ nóng.

## Điều đã được kiểm chứng trong code
- Bốn trạng thái pha RUN_PUMP_PREDELAY/RUN_ACTIVE/PRESET_RECORD_ACTIVE/CLEAN_FLUSH giữ SSR HIGH với nhiệt hợp lệ, cả khi pump OFF (56 tổ hợp state/temp/pump).
- Các trạng thái trên khi kết thúc với filtered < set yêu cầu SSR HIGH ngay, không phụ thuộc latch thermostat cũ (56 tổ hợp state/temp/latch, gồm cả end >=set để kiểm tra OFF).
- Bộ 101 case đã chạy trước audit trên đúng SHA: normal và ASan/UBSan PASS, target ESP8266 build PASS. Audit mới thêm 6 nhóm, tất cả PASS; PASS ở các phản ví dụ nghĩa là tái hiện được hành vi còn tồn tại, không phải đã loại bỏ rủi ro.
- Các exit route thực (timeout, hoàn thành, SET/RUN hủy, record/save, clean) nằm trong bộ hồi quy trước đó; bảng tổ hợp mới kiểm tra trực tiếp helper, không thay thế end-to-end.
- Chưa đo ESP/SSR/dòng điện/nhiệt thực; hardware gate chưa chạy. Firmware production không đổi trong audit.

## Hướng xử lý
Nếu mục tiêu là tắt ngay khi end >=set nhưng bật lại ngay khi số đo xuống dưới set, nên giữ một trạng thái giám sát sau pha thay vì xóa ngay khi gặp số đo nóng. Cần chốt tiêu chí kết thúc giám sát để không rơi lại vào cùng khoảng chờ; không tự thêm thời gian đun cưỡng bức hoặc bỏ bảo vệ. Đây là đề xuất, chưa implement.
Ngay cả bật lại dưới set vẫn không loại bỏ trễ vật lý nếu NTC báo nóng trong khi nước đã nguội.
Trace bench cần đồng thời: thời gian, FSM, pump, D7, raw calibrated, filtered, setpoint, recoveringHeat_, fault, tuổi mẫu; đối chiếu nhiệt độc lập và công suất thực.
- D7 LOW khi filtered còn cao: F1/lag; raw hoặc filtered >145: F3.
- D7 LOW khi filtered <set trong recovery, valid/fresh và cả hai <=145: trái invariant, cần điều tra trace/firmware thực.
- D7 HIGH nhưng nhiệt vẫn giảm: kiểm tra nhiệt cấp so với lưu lượng, SSR thực dẫn và đo nhiệt; không thể giải quyết bằng yêu cầu ON cao hơn HIGH.
