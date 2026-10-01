# Sửa lỗi ẩn v14 — v15, 2026-10-01

Baseline: v14 commit75e184c. Bản sửa được duyệt để commit/push lên origin/main. Chưa nạp ESP; G10 NOT RUN.

| Lỗi | Sửa | Test |
|---|---|---|
| 3s trong vùng set..set+0.5 dù nhiệt đang giảm | Reset cửa sổ nếu range >0.10 hoặc giảm từ peak >0.05°C; chỉ handoff sau3s mẫu mới | H20 giảm0.49/0.09, H21 plateau nhiễu0.04, H22 tăng0.3 |
| Setpoint chỉ đổi sau commit, SSR cònON theo target cũ | Set target, clear latch, applyHeating trước flash; lỗi commit vẫnE6 | H23 thường/monitor; H24 fail-closed |
| Save sát timeout20s bị hủy | Hai nút đang giữ cập nhật last-activity | H25 debounce thật, save bắt đầu19.875s |
| READY transition làm mất hold/release | enterState thay enterIdle chỉ ở HEATING↔READY; xử lý gesture cùng tick | H26 cảhai chiều, H27 edit, H28 shortclean |

114/114 normal và ASan/UBSan PASS. Cùng114case trên snapshotv14:7fail ở mỗi mode (H20/H22/H23/H25/H26/H27/H28). Test H20/H23/H26 chứa thêm nhiều kịch bản trong một case, không coi7case là7defect độc lập.
Test mới nằm trong test/host_review/heat_recovery_cases.h. Reports: host-gate-results.json, v14-hidden-reproduction.json, target-build-results.json.
Giữ bảo vệ145°C, calibration+15, EEPROMv5 và thủy lực. Exact setpoint switching có thể rung mức nếu filtered nhiễu quanhset; lượt này không tự thêm dwell/heat boost.
Giới hạn: .10/.05 là tham số phần mềm cần bench; không thể phân biệt hoàn hảo nhiễu với trôi nhiệt nhỏ. Một thay đổi nhiệt xảy ra sau khi đãhandoff vẫn chịu thermostatv11. G10NOT RUN.
