# Arduino six-step release set

ชุดนี้มีสเก็ตช์ STM32F405 สำหรับบอร์ด ESC V02 จำนวน 3 รุ่น เลือกแฟลชเพียงหนึ่งรุ่นให้ตรงกับงาน ห้ามรวมไฟล์ข้ามโฟลเดอร์

ดาวน์โหลดแยก Release ตามรุ่น: [สำหรับแอป v33-proto1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-app-v33-proto1) · [วัดกระแส v1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-current-v1) · [ไม่วัดกระแส v1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-no-current-v1) แอป Windows อยู่ที่ [SAT motor pilot v1.0.0](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.0.0-app)

แต่ละ Firmware Release มีไฟล์ `.elf` สำหรับแฟลชผ่าน ST-LINK/SWD และ ZIP ซอร์สโค้ด อ่าน [วิธีเลือกไฟล์และแฟลช ELF อย่างปลอดภัย](ELF_FLASH_GUIDE_TH.md) ก่อนใช้งาน

| รุ่น | ใช้เมื่อ | การวัดกระแส | ใช้กับ SAT motor pilot |
|---|---|---|---|
| `stm32f405_sixstep_current` | bring-up ผ่าน Serial Monitor และต้องการ software current trip | เปิด ใช้ PC0/PC1/PC2 | ไม่รองรับ |
| `stm32f405_sixstep_no_current` | ตรวจ gate/ลำดับ six-step หรือบอร์ดที่ยังไม่ติดตั้ง current sense | ปิดทั้งการอ่านและ trip | ไม่รองรับ |
| `stm32f405_sixstep_app_v33` | ใช้แอป SAT motor pilot ผ่าน USB CDC | diagnostic เท่านั้น, protection ปิด | รองรับ `v33-proto1` |

> ทั้งสามรุ่นเป็น firmware สำหรับทดลองบนโต๊ะ ไม่ใช่เฟิร์มแวร์ production และไม่มีรุ่นใดทดแทนวงจร over-current shutdown, ฟิวส์, แหล่งจ่ายจำกัดกระแส หรือสวิตช์ตัด DC bus ได้

## ตั้งค่า Arduino IDE

1. ติดตั้ง Arduino IDE 2 และแพ็กเกจ **STM32 MCU based boards** ของ STMicroelectronics
2. เลือก `Generic STM32F4 series`
3. เลือก part number `Generic F405RGTx`
4. เลือก USB support `CDC (generic Serial supersede U(S)ART)` และ USB speed `Low/Full Speed`
5. ถ้าใช้ ST-LINK ให้เลือก upload method `STM32CubeProgrammer (SWD)`
6. เปิดไฟล์ `.ino` ที่ชื่อเดียวกับโฟลเดอร์ กด Verify ก่อน แล้วจึง Upload
7. เปิด Serial Monitor ที่ 115200 baud และ line ending เป็น `Newline`

## เลือกรุ่นอย่างรวดเร็ว

### 1. รุ่นวัดกระแส

เปิด `stm32f405_sixstep_current/stm32f405_sixstep_current.ino` รุ่นนี้วัด zero ตอนเปิดเครื่องและมี software trip ค่าเริ่มต้นประมาณ 2 A จากสมมติฐาน INA181A1 gain 20, shunt 1 mΩ และ ADC 3.3 V ต้องสอบเทียบกับมิเตอร์และฮาร์ดแวร์จริงก่อนเชื่อค่ากระแส

ลำดับเริ่มต้น: `duty 8` → `period 10000` → `arm` → `run` และใช้ `stop` เพื่อหยุดทันที ดูรายละเอียดใน `stm32f405_sixstep_current/README_TH.md`

### 2. รุ่นไม่วัดกระแส

เปิด `stm32f405_sixstep_no_current/stm32f405_sixstep_no_current.ino` รุ่นนี้ไม่อ่าน PC0/PC1/PC2 และไม่มี software current trip คำสั่ง `trip` และ `zero` จะถูกปฏิเสธ ใช้แหล่งจ่ายจำกัดกระแสภายนอกเสมอ

ลำดับเริ่มต้นเหมือนรุ่นวัดกระแส แต่ควรเริ่มโดยถอดมอเตอร์ ใช้ `test` ตรวจ HIN/LIN และ dead time ด้วยออสซิลโลสโคปก่อน

### 3. รุ่นสำหรับแอป

เปิด `stm32f405_sixstep_app_v33/stm32f405_sixstep_app_v33.ino` แล้วแฟลช จากนั้นเปิด SAT motor pilot แอปจะยอมเชื่อมต่อเมื่อ `HELLO` รายงาน `fw=v33-proto1`, `protocol=1` และ `current_protection=OFF` เท่านั้น

รุ่นนี้ต้องได้รับ `HEARTBEAT` จากแอปอย่างต่อเนื่อง ไม่ควรใช้ Serial Monitor สั่ง `RUN` แทนแอป การอ่านกระแสเป็น diagnostic ที่ยังไม่ผ่านการยืนยันและไม่ได้ใช้เป็น current trip

## ขั้นตอนทดสอบที่ปลอดภัย

1. แฟลชโดยยังไม่ต่อมอเตอร์และไม่จ่าย DC bus
2. ยืนยันว่า gate input ทั้งหกเป็น LOW หลัง reset
3. ใช้ `test` หรือการทดสอบจากคู่มือของรุ่นนั้น ตรวจว่า HIN/LIN เฟสเดียวกันไม่ HIGH พร้อมกัน
4. ใช้ differential/isolated probe เมื่อวัด high-side gate ห้ามต่อ ground clip เข้ากับ phase node หรือ HO
5. ต่อมอเตอร์เมื่อผ่านการตรวจ logic แล้ว ใช้แรงดันต่ำและ current limit ต่ำที่สุดที่ทำงานได้
6. เตรียมสวิตช์ตัด DC bus และกด `stop` ทันทีเมื่อมอเตอร์สั่น ค้าง ร้อน หรือกระแสเพิ่มผิดปกติ

## VescUart

ชุดนี้ไม่ใช้ไลบรารี `SolidGeek/VescUart` เพราะไลบรารีนั้นสื่อสารด้วย packet protocol ของ VESC ขณะที่ SAT motor pilot v33 ใช้คำสั่ง ASCII เฉพาะของสเก็ตช์ผ่าน USB CDC การใส่ VescUart จะไม่ทำให้สองโปรโตคอลเข้ากันและเพิ่ม dependency ที่ไม่จำเป็น

## ทรัพยากรและ timing

- รุ่น current/no-current ใช้ TIM1 PWM 30 kHz, IWDG ประมาณ 1 วินาที และเก็บ state ในตัวแปรคงที่ ไม่มี dynamic allocation; หาก IWDG เริ่มไม่ได้ firmware จะไม่ยอม arm
- รุ่น app ใช้ TIM1 PWM 20 kHz, watchdog ประมาณ 1 วินาที, heartbeat timeout 1500 ms และบัฟเฟอร์ Scope คงที่ 512 ตัวอย่าง (`ScopeSample` 12 ไบต์โดย alignment ปัจจุบัน หรือประมาณ 6 KiB RAM)
- แอปเก็บกราฟย้อนหลังบนคอมพิวเตอร์ ไม่เพิ่ม RAM history บนบอร์ดนอกเหนือจาก scope buffer
- ผล Verify ด้วย STM32duino 3.0.0 ของทั้งสามรุ่นและขนาด Flash/RAM อยู่ใน [คู่มือ ELF](ELF_FLASH_GUIDE_TH.md) แต่ยังไม่ได้แฟลชหรือวัด timing บนบอร์ดจริง
- ตรวจ jitter, ADC sample point, gate dead time และ timeout ด้วย oscilloscope/logic analyzer บนบอร์ดจริง การ compile อย่างเดียวไม่ยืนยัน timing ของภาคกำลัง
