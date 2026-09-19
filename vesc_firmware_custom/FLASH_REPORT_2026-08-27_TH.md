# รายงานการแฟลช BLDC_LITE_24

- เวลาแฟลช: 2026-08-27 ประมาณ 15:47 (Asia/Bangkok)
- Device ID: `0x413`
- Device: STM32F405xx/F407xx/F415xx/F417xx
- Flash: 1 MiB
- Target voltage: 3.24–3.25 V
- Firmware: `output/bldc_lite_24/bldc_lite_24.bin`
- Firmware SHA-256: `2BCCD8982C429F781BCE254A43C039D39A031C9D20106B259F940809CB76A11E`
- Address: `0x08000000`
- STM32CubeProgrammer download: สำเร็จ
- STM32CubeProgrammer verify ก่อน reset: สำเร็จ
- MCU reset: สำเร็จ

## สำรองก่อนแฟลช

มีไฟล์สำรอง 1 MiB สองชุดก่อนเขียน ทั้งสองชุดมี SHA-256 เดียวกัน:

`169753AB5B4E065E2264B0542E83C466F4B542646B957A0F9802829DB7FFD00E`

- `backups/stm32f405_before_vesc_20260827_154558.bin`
- `backups/stm32f405_before_vesc_20260827_154710.bin`

## การอ่านกลับหลังบูต

- Readback: `backups/verification_readback_20260827_1547.bin`
- Readback SHA-256: `B339595BF6B63D19154CB4F5CDCAFF87E9A0E8DB541CC67208323458A7F95DD0`
- ส่วนโปรแกรม `0x08000000–0x08003FFF` และ `0x0800C000–0x0807FFEF` ตรงกับไฟล์ที่บิลด์ทุกไบต์
- Sector 1 (`0x08004000–0x08007FFF`) ถูกใช้เป็น EEPROM emulation และมี config เริ่มต้นหลังบูต
- Sector 2 (`0x08008000–0x0800BFFF`) เป็นหน้า EEPROM สำรองและอยู่ในสถานะ erased
- CRC info ที่ `0x0807FFF0–0x0807FFF7` เปลี่ยนจาก `FF FF FF FF FF FF FF FF` เป็น `00 00 00 00 CC F7 F2 1E` หลัง firmware คำนวณ CRC

ความแตกต่างหลังบูตจึงอยู่เฉพาะ EEPROM emulation และ CRC info ตามโครงสร้าง VESC ไม่มีความต่างใน code region

## สถานะล่าสุด

- เฟิร์มแวร์เริ่มต้นแบบ `DISARMED`
- ผู้ใช้ยืนยันว่าถอดมอเตอร์และปิดหรือจำกัดกำลัง VSUPPLY ก่อนแฟลช
- ยังไม่พบ VESC USB device บนเครื่อง จึงยังไม่ได้รัน `bldc_hw_status`
- ห้ามต่อมอเตอร์หรือสั่ง `bldc_arm CONFIRM` จนกว่าจะตรวจ ADC และ gate waveform
