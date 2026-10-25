# Arduino six-step app firmware v33-proto1

ไฟล์ใน Release นี้สำหรับ STM32F405RGT6:

- `stm32f405-sixstep-app-v33-proto1.zip` — ซอร์สโค้ด
- `stm32f405-sixstep-app-v33-proto1.elf` — ELF พร้อมแฟลชผ่าน ST-LINK/SWD

SHA-256: `758403ce445c3b18958c682dbf212102430607e5f3b37db176dd74ac7d5526dd`

ELF SHA-256: `60deb4fdfb3a3460cb09b620f5169fd227439173d149345b10682884fb420847`

รุ่นนี้ใช้กับ [SAT motor pilot v1.0.0](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.0.0-app) ผ่าน USB CDC โดยรายงาน `fw=v33-proto1`, `protocol=1`, `current_protection=OFF` มี heartbeat และ telemetry **ไม่ใช่** รุ่น software current trip

วิธีแฟลชและใช้งาน: [คู่มือ Arduino six-step](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/README_TH.md)

วิธีแฟลช ELF: [คู่มือ ST-LINK/SWD](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/ELF_FLASH_GUIDE_TH.md) สร้างด้วย STM32duino 3.0.0; Verify ผ่าน แต่ยังไม่ได้ทดสอบกับบอร์ดจริง

> เป็น firmware ทดลอง ไม่ผ่านการรับรองกับภาคกำลังจริง การอ่านกระแสยังเป็น diagnostic ที่ไม่ยืนยันและไม่ใช้ตัดกระแส ใช้แหล่งจ่ายจำกัดกระแส ฟิวส์ และสวิตช์ตัด DC bus
