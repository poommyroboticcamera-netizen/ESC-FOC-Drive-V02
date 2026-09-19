# Contributing to ESC-FOC-Drive-V02

ขอบคุณที่สนใจพัฒนาโปรเจกต์นี้ การเปลี่ยนแปลง motor-control firmware และ power electronics มีผลต่อความปลอดภัยโดยตรง จึงต้องมีข้อมูลที่ตรวจสอบย้อนกลับได้มากกว่าโปรเจกต์ซอฟต์แวร์ทั่วไป

## Before opening a change

1. เปิด issue เพื่ออธิบายปัญหา, board revision, firmware commit และผลที่คาดหวัง
2. แยกการเปลี่ยนแปลง firmware, hardware design และ generated artifacts ออกจากกันให้ชัดเจน
3. อย่า commit credentials, device backups, build directories, binary firmware หรือไฟล์ทดสอบขนาดใหญ่
4. ห้ามเพิ่ม current, duty, voltage หรือ disable protection โดยไม่มีเหตุผลทางวิศวกรรมและแผนทดสอบ

## Development workflow

```sh
git switch -c feature/short-description
git status
git diff --check
```

ใช้ commit message ที่ระบุผลกระทบ เช่น `motor: tighten raw over-current cutoff` หรือ `docs: record rev-B current-sense calibration` และเก็บหนึ่งประเด็นหลักต่อ commit เมื่อทำได้

## Evidence required

| Change type | Minimum evidence |
|:--|:--|
| Documentation | Links, paths and revision identifiers verified |
| Firmware logic | Build result and relevant host-side tests |
| Pin mapping | Schematic page, MCU pin and peripheral cross-check |
| Current sensing | Shunt value, amplifier gain, polarity, offset and test current |
| PWM / gate drive | PWM frequency, dead time, probe method and waveform capture |
| Protection limit | Trigger condition, sampling behavior and fail-safe result |
| Hardware design | Updated schematic/BOM/Gerber set and independent CAM review |

## Pull-request checklist

- [ ] Target board and revision are identified.
- [ ] Safety limits are unchanged or the reason and evidence are documented.
- [ ] Motor-disconnected checks are described before powered tests.
- [ ] Generated files and local backups are excluded.
- [ ] README or engineering records are updated when behavior changes.
- [ ] A rollback or recovery path is documented for flash-related changes.

Do not describe a test as passed unless the exact firmware commit, hardware revision, supply limit, motor state and measurement method are known.
