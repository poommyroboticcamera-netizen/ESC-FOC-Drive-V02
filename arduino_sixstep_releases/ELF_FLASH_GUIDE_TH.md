# แฟลช Arduino six-step ELF สำหรับ ESC-FOC-Drive-V02

ไฟล์ ELF ทั้งสามสร้างจากสเก็ตช์ใน repository นี้สำหรับ **STM32F405RGT6 เท่านั้น** ด้วย Arduino CLI 1.2.0, STM32 MCU based boards 3.0.0, GCC 14.2.1, `Generic STM32F4 series / Generic F405RGTx`, USB `CDC (generic Serial supersede U(S)ART)`, USB speed `Low/Full Speed`, optimization `-Os` และ ST-LINK/SWD เลือกแฟลช **เพียงหนึ่งรุ่น**:

| ELF ใน Release | หน้าที่ | Flash จากผล Verify | Global RAM จากผล Verify |
|---|---|---:|---:|
| `stm32f405-sixstep-app-v33-proto1.elf` | ใช้คู่กับ SAT motor pilot เท่านั้น; current protection ปิด | 68,368 B | 13,976 B |
| `stm32f405-sixstep-current-v1.elf` | Serial Monitor; วัดกระแส + software trip | 45,452 B | 4,976 B |
| `stm32f405-sixstep-no-current-v1.elf` | Serial Monitor; ไม่วัดกระแส/ไม่มี trip | 41,496 B | 4,960 B |

ตำแหน่ง vector table ใน ELF ทุกตัวคือ `0x08000000` (`ELF32`, ARM) ไฟล์ ELF มีตำแหน่งหน่วยความจำอยู่แล้ว ไม่ต้องใส่ start address แบบไฟล์ `.bin` ไฟล์นี้ไม่ใช่เฟิร์มแวร์ VESC และห้ามใช้กับ MCU/บอร์ดคนละรุ่น

## ขั้นตอนผ่าน STM32CubeProgrammer

1. **ตัด DC bus และถอดมอเตอร์** ก่อนต่อ ST-LINK ห้ามให้ภาคกำลังเริ่มทำงานระหว่างแฟลช
2. ตรวจตัวอักษรบน MCU ว่าเป็น STM32F405RGT6 และต่อ ST-LINK กับ SWDIO, SWCLK, GND, VTref ตามบอร์ด ตรวจแรงดันเลี้ยง logic ก่อนเชื่อมต่อ
3. เปิด STM32CubeProgrammer เลือก ST-LINK/SWD และ Connect ตรวจว่าเครื่องมืออ่าน target ตรงกับ STM32F405 และ Flash 1 MiB
4. เลือกไฟล์ `.elf` จาก [Release รุ่นที่ต้องการ](README_TH.md) ในหน้า Erasing & Programming เปิด **Verify after programming** แล้ว Start Programming อย่าเลือก Skip flash erase
5. หลัง Verify ผ่าน ให้รีเซ็ตบอร์ดโดย **ยังไม่จ่าย DC bus** ตรวจว่า HIN/LIN ทั้งหก LOW และไม่มี fault/ความร้อนผิดปกติ
6. สำหรับรุ่น app ให้ต่อ USB และตรวจ `HELLO` ผ่าน SAT motor pilot (`fw=v33-proto1`, `protocol=1`, `current_protection=OFF`) ก่อนทดสอบใด ๆ รุ่น current/no-current ใช้ Serial Monitor ตามคู่มือของรุ่นนั้น ห้ามใช้กับแอป
7. ก่อนต่อมอเตอร์ วัด gate waveform และ dead time ด้วยเครื่องมือที่เหมาะสม จากนั้นทดสอบด้วยแหล่งจ่ายแรงดันต่ำที่จำกัดกระแส พร้อมฟิวส์และสวิตช์ตัด DC bus

ทางเลือก CLI เมื่อยืนยัน target แล้ว: `STM32_Programmer_CLI -c port=SWD -w <ไฟล์.elf> -v` (`-v` ต้องอยู่หลัง `-w`) **อย่ารันคำสั่งนี้กับบอร์ดที่ยังต่อภาคกำลัง/มอเตอร์อยู่**

การคอมไพล์และตรวจโครงสร้าง ELF ผ่านแล้ว แต่ **ยังไม่ได้แฟลชและทดสอบกับบอร์ดจริง** จึงไม่ยืนยันการ enumerate ของ USB CDC, timing, gate waveform, current trip หรือการหยุดเมื่อ fault ซอฟต์แวร์ไม่ทดแทน hardware over-current shutdown
