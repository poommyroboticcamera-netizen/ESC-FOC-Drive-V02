# STM32F405 six-step แบบไม่วัดกระแส

สเก็ตช์ `sixstep-no-current-v1` ใช้ทดสอบลำดับ six-step, สัญญาณ HIN/LIN และการหมุน open-loop บนบอร์ด ESC V02 โดย **ไม่อ่าน PC0/PC1/PC2 และไม่มี software current trip** เหมาะกับการตรวจ logic หรือบอร์ดที่ยังไม่ประกอบ INA181/shunt เท่านั้น

เฟิร์มแวร์นี้ไม่ใช่ FOC, ไม่ใช่ closed-loop speed controller และไม่ใช่ระบบป้องกันภาคกำลัง

## ตั้งค่า

- Arduino IDE 2 + STM32 MCU based boards
- Board: `Generic STM32F4 series`
- Part number: `Generic F405RGTx`
- USB: `CDC (generic Serial supersede U(S)ART)`, Low/Full Speed
- Upload: `STM32CubeProgrammer (SWD)` เมื่อใช้ ST-LINK
- Serial Monitor: 115200 baud, Newline

## คำสั่ง

| คำสั่ง | การทำงาน |
|---|---|
| `arm` | ปิดเกตทั้งหมดแล้วเข้าสถานะพร้อม |
| `test` | six-step ช้าเพื่อวัดสัญญาณ; ต้องถอดมอเตอร์ |
| `run` | หมุน open-loop พร้อม ramp คาบ commutation |
| `stop` | ปิด gate input ทั้งหกและยกเลิก arm |
| `duty N` | ตั้ง duty 1–15%, ค่าเริ่มต้น 8% |
| `period N` | ตั้งคาบต่อ step 1,000–100,000 µs |
| `dir fwd` / `dir rev` | เปลี่ยนทิศขณะหยุด |
| `status` | แสดง state, duty, phase ADC และยืนยันว่า current sense ปิด |
| `trip` / `zero` | ปฏิเสธเสมอในรุ่นนี้ |

## วิธีทดสอบ

1. แฟลชโดยยังไม่ต่อมอเตอร์และไม่จ่าย DC bus
2. ตรวจว่า PA8/PA9/PA10 และ PB13/PB14/PB15 เป็น LOW หลัง reset
3. ส่ง `arm` แล้ว `test` ตรวจลำดับที่ H2 และยืนยันว่า HIN/LIN ของเฟสเดียวกันไม่ HIGH พร้อมกัน
4. ส่ง `stop` ก่อนต่อหรือถอดสายทุกครั้ง
5. เมื่อต่อมอเตอร์ ให้ใช้แหล่งจ่ายจำกัดกระแสภายนอก, ฟิวส์ และสวิตช์ตัด DC bus เริ่มด้วย `duty 8`, `period 10000`, `arm`, `run`
6. หากมอเตอร์สั่น ค้าง หรือร้อน ให้ `stop` และตัด DC bus ห้ามเพิ่ม duty เพื่อฝืนให้หมุน

การไม่มี current sense ทำให้ซอฟต์แวร์ไม่เห็น over-current ใด ๆ ปุ่ม SW3/SW4 และคำสั่ง STOP ไม่ใช่วงจรป้องกัน short circuit แบบฮาร์ดแวร์
