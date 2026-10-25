# Arduino six-step no-current v1

ไฟล์ใน Release นี้สำหรับ STM32F405RGT6:

- `stm32f405-sixstep-no-current-v1.zip` — ซอร์สโค้ด
- `stm32f405-sixstep-no-current-v1.elf` — ELF พร้อมแฟลชผ่าน ST-LINK/SWD

SHA-256: `9a57f37e053ec0bff5b0c67428f8f66daae1b0deab15f5dc46875e5c91177f7b`

ELF SHA-256: `ff6a1236c8cf115dfffd15583ff3ee3a956b4d22286ff01be22075af2034c507`

รุ่น bring-up ผ่าน Serial Monitor **ไม่อ่าน** PC0/PC1/PC2 และ **ไม่มี** current feedback หรือ software current trip **ไม่รองรับ SAT motor pilot** เหมาะกับการตรวจ logic/gate ในสภาพทดสอบที่จำกัดกระแสภายนอก

วิธีแฟลชและใช้งาน: [คู่มือ Arduino six-step](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/README_TH.md)

วิธีแฟลช ELF: [คู่มือ ST-LINK/SWD](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/ELF_FLASH_GUIDE_TH.md) สร้างด้วย STM32duino 3.0.0; Verify ผ่าน แต่ยังไม่ได้ทดสอบกับบอร์ดจริง

> เป็น firmware ทดลอง ไม่ผ่านการรับรองกับภาคกำลังจริง ใช้แหล่งจ่ายจำกัดกระแส ฟิวส์ และสวิตช์ตัด DC bus
