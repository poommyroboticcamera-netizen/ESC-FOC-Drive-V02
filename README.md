# ESC-FOC-Drive-V02

เอกสารและซอร์สโค้ดสำหรับการ bring-up และพัฒนาไดรฟ์มอเตอร์ BLDC/PMSM แบบสามเฟสบนบอร์ด STM32F405 พร้อมชุดทดลอง Arduino สำหรับตรวจภาค gate driver ก่อนใช้เฟิร์มแวร์ FOC

> **คำเตือนด้านความปลอดภัย:** โค้ดและการตั้งค่าในรีโปนี้ใช้สำหรับงานพัฒนาและทดสอบโดยผู้ที่มีความชำนาญเท่านั้น ภาคกำลังมอเตอร์อาจทำให้เกิดกระแสสูง แรงดันย้อนกลับ ความร้อน และการเคลื่อนที่โดยไม่คาดคิดได้ ห้ามใช้เฟิร์มแวร์หรือชุดทดสอบนี้ในอุปกรณ์ที่เกี่ยวข้องกับความปลอดภัย และต้องใช้แหล่งจ่ายจำกัดกระแส ฟิวส์ และเครื่องมือวัดที่เหมาะสมเสมอ

## โครงสร้างรีโป

| โฟลเดอร์ | เนื้อหา |
| --- | --- |
| `vesc_firmware_custom/` | เฟิร์มแวร์ VESC ที่ปรับแต่งสำหรับบอร์ดนี้ รวมการแก้ไขส่วน motor control และเอกสารการ flash/calibration ภาษาไทย |
| `arduino_stm32f405_bldc_test/` | Arduino sketch สำหรับ STM32F405RGT6: six-step open-loop, PWM 30 kHz เพื่อ bring-up ภาค gate และมอเตอร์ที่แรงดัน/กระแสต่ำ |
| `arduino_eg2132_bldc_test/` | Arduino Uno/Nano sketch สำหรับทดสอบ gate driver EG2132 แบบ six-step open-loop |
| `hardware/` | BOM, Pick-and-Place, schematic PDF และ Gerber package ของบอร์ด BLDC V02 |

ไฟล์ผลทดสอบ, binary backup, build directory และไฟล์ชั่วคราวจะไม่ถูกเก็บใน Git เพื่อให้รีโปมีเฉพาะซอร์สและเอกสารที่ทำซ้ำได้

## เริ่มต้นใช้งาน

1. อ่าน README ในโฟลเดอร์ชุดทดสอบที่ต้องการก่อนต่อวงจร:
   - [`arduino_stm32f405_bldc_test/README_TH.md`](arduino_stm32f405_bldc_test/README_TH.md)
   - [`arduino_eg2132_bldc_test/README_TH.md`](arduino_eg2132_bldc_test/README_TH.md)
2. เริ่มด้วยการตรวจสัญญาณที่ HIN/LIN โดย **ไม่ต่อมอเตอร์** และยังไม่จ่าย `VSUPPLY`.
3. ตรวจ dead time, gate waveform และยืนยันว่า high-side/low-side ของเฟสเดียวกันไม่เปิดพร้อมกัน.
4. เมื่อต้องทดสอบกับมอเตอร์ ให้ใช้แรงดันต่ำที่สุดและ current limit ต่ำก่อนเสมอ.

แบบวงจรและไฟล์ผลิต PCB อยู่ใน `hardware/`; ตรวจสอบ revision ของ BOM, schematic และ Gerber ให้ตรงกันทุกครั้งก่อนสั่งผลิตหรือประกอบ

## เฟิร์มแวร์ FOC

โค้ดหลักอยู่ที่ `vesc_firmware_custom/` ซึ่งมาจากฐานโค้ด [VESC firmware](https://github.com/vedderb/bldc) และมีการปรับแต่งเฉพาะบอร์ด โปรดอ่านเอกสารในโฟลเดอร์นี้ เช่น `BUILD_INFO_TH.md`, `FLASH_AND_FIRST_TEST_TH.md`, `CALIBRATION_2026-09-07_TH.md` และ `README_BLDC_LITE_TH.md` ก่อน build หรือ flash

การ build/flash ต้องเลือก target และ hardware configuration ให้ตรงกับบอร์ดจริง การใช้ไฟล์ binary หรือพารามิเตอร์จากบอร์ดอื่นอาจทำให้ MOSFET, gate driver หรือมอเตอร์เสียหายได้

## ข้อจำกัดของชุดทดสอบ Arduino

สเก็ตช์ Arduino ทั้งสองชุดเป็น six-step open-loop สำหรับตรวจสอบวงจรและ bring-up เท่านั้น ไม่ใช่ FOC และไม่มี hardware over-current shutdown ที่เพียงพอสำหรับใช้งานจริง Software trip หรือปุ่มหยุดไม่สามารถใช้แทนวงจรป้องกัน short-circuit แบบฮาร์ดแวร์ได้

## ที่มาและสัญญาอนุญาต

`vesc_firmware_custom/` อิงจาก VESC firmware ของ Benjamin Vedder; ให้ตรวจสอบไฟล์ license และเอกสารของ upstream ในซอร์สโค้ดก่อนนำไปแจกจ่ายหรือใช้เชิงพาณิชย์ การปรับแต่งเฉพาะโครงการและชุดทดสอบในรีโปนี้จัดทำโดยผู้ดูแลรีโปนี้

## การมีส่วนร่วม

ก่อนส่งการแก้ไข กรุณาอย่า commit ไฟล์ build, firmware binary, backup ที่มีข้อมูลเฉพาะอุปกรณ์ หรือผลทดสอบขนาดใหญ่ หากเพิ่มการตั้งค่าฮาร์ดแวร์ ให้บันทึกรุ่นบอร์ด, revision, เงื่อนไขทดสอบ และขั้นตอน rollback ไว้ในเอกสารประกอบ
