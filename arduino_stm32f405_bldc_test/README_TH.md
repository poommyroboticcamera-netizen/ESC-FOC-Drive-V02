# โค้ดทดสอบ BLDC สำหรับ STM32F405RGT6

สเก็ตช์นี้เขียนตาม `SCH_drive_2026-08-24.pdf` สำหรับ MCU บนบอร์ดคือ STM32F405RGT6 ใช้ Arduino IDE 2 และ STM32duino สร้าง PWM 30 kHz ที่ TIM1 แล้วขับแบบ six-step open-loop เพื่อทดสอบภาคเกตและหมุนมอเตอร์เบื้องต้น

นี่เป็นโค้ด bring-up บนโต๊ะทดลอง ไม่ใช่ FOC และไม่ใช่เฟิร์มแวร์พร้อมใช้งานจริง

## ขาที่ใช้ตามวงจร

| หน้าที่ | Net ในวงจร | STM32F405 |
|---|---|---|
| High-side U | UH -> U_HIN | PA8 / TIM1_CH1 |
| High-side V | VH -> V_HIN | PA9 / TIM1_CH2 |
| High-side W | WH -> W_HIN | PA10 / TIM1_CH3 |
| Low-side U | UL -> U_LIN | PB13 |
| Low-side V | VL -> V_LIN | PB14 |
| Low-side W | WL -> W_LIN | PB15 |
| Current U/V/W | CURRENT_U/V/W | PC0 / PC1 / PC2 |
| Phase voltage U/V/W | UV / VV / WV | PA0 / PA1 / PA2 |
| ปุ่มหยุดระหว่างทดสอบ | SW3 / SW4 | PC4 / PC5 |

หัว H2 เป็นจุดวัดสัญญาณเดียวกันกับเกตอินพุตและ current sense ไม่ต้องต่อ Arduino Uno เพิ่ม

## ตั้งค่า Arduino IDE

1. ใช้ Arduino IDE 2 และติดตั้งแพ็กเกจ `STM32 MCU based boards` ของ STMicroelectronics ผ่าน Boards Manager
2. เลือก Board: `Generic STM32F4 series`
3. เลือก Board part number: `Generic F405RGTx`
4. เลือก USB support: `CDC (generic Serial supersede U(S)ART)`
5. เลือก USB speed: `Low/Full Speed`
6. ถ้าโปรแกรมผ่านหัว H19 ด้วย ST-LINK ให้เลือก Upload method: `STM32CubeProgrammer (SWD)`
7. เปิดไฟล์ `arduino_stm32f405_bldc_test.ino`, Compile และ Upload
8. เปิด Serial Monitor ที่ 115200 baud และเลือก line ending เป็น `Newline`

โค้ดตรวจสอบการคอมไพล์กับ STM32duino core 2.10.1 และ target `GENERIC_F405RGTX` แล้ว

## คำสั่ง Serial

| คำสั่ง | การทำงาน |
|---|---|
| `arm` | ปิดทุกเกต, วัด current zero แล้วเข้าสถานะพร้อม |
| `test` | วน six-step ช้า ๆ สำหรับวัดที่ H2/เกต ต้องถอดมอเตอร์ |
| `run` | เริ่มหมุนแบบ open-loop พร้อม ramp ความเร็ว |
| `stop` | ปิดอินพุตเกตทั้งหกทันทีและยกเลิก arm |
| `duty N` | ตั้ง PWM 1-15% ค่าเริ่มต้น 8% |
| `period N` | ตั้งคาบต่อหนึ่ง commutation step 1000-100000 us |
| `dir fwd` / `dir rev` | เปลี่ยนทิศขณะหยุด |
| `trip N` | ตั้ง software current trip เป็นผลต่าง ADC counts; ประมาณ 24.8 counts/A |
| `trip 0` | ปิด software trip ใช้เฉพาะ logic/scope test ที่ภาคกำลังปลอดภัย |
| `zero` | วัด current zero ใหม่ขณะหยุด |
| `status` | อ่าน current, phase voltage และสถานะ |
| `help` | แสดงคำสั่งทั้งหมด |

SW3 หรือ SW4 จะสั่งหยุดเมื่ออยู่ในสถานะ ARMED/RUN/TEST แต่ปุ่มและ software trip ไม่ใช่วงจรป้องกัน short-circuit แบบฮาร์ดแวร์

## ลำดับทดสอบที่ปลอดภัยกว่า

1. ยังไม่ต่อมอเตอร์และยังไม่จ่าย VSUPPLY อัปโหลดโค้ดก่อน ทุกอินพุตเกตต้องเป็น LOW หลัง reset
2. U13-U15 ยืนยันเป็น INA181A1 gain 20 V/V และ shunt 1 mOhm; ค่า trip เริ่มต้นประมาณ 2 A แต่ยังเป็น software trip ที่ตอบสนองช้า
3. ตรวจ H2 ว่าลำดับ six-step ถูกต้อง และไม่พบ HIN/LIN ของเฟสเดียวกัน HIGH พร้อมกัน
4. ถ้าจะวัด HO ให้จ่ายไฟบอร์ดผ่านแหล่งจ่ายจำกัดกระแสและตรวจว่าราง 12V/5V/3.3V ถูกต้องก่อน ห้ามต่อมอเตอร์ในรอบนี้
5. วัด HO เทียบกับ VS ของเฟสนั้นด้วย differential probe หรือ isolated probe ห้ามต่อ ground clip ของออสซิลโลสโคปลง PHASE_U/V/W หรือ HO
6. ก่อนต่อมอเตอร์ ส่ง `stop`, ตั้ง current limit ต่ำ, ใช้ VSUPPLY ต่ำที่สุดที่ยังทำให้ภาคจ่ายไฟและ gate driver ทำงานถูกต้องตามสเปกอุปกรณ์จริง
7. เริ่มด้วย `duty 8`, `period 10000`, `arm`, `run` หากมอเตอร์สั่นหรือไม่หมุนให้ `stop` ก่อน แล้วลอง `dir rev` หรือสลับสายมอเตอร์สองเส้น

## จุดที่ควรแก้วงจรก่อนทดสอบภาคกำลัง

- เพิ่ม pull-down ประมาณ 10 kOhm ที่ U_HIN, V_HIN, W_HIN, U_LIN, V_LIN และ W_LIN เพราะตอน reset/programming ขา MCU เป็น high-impedance และในแบบปัจจุบันมีเพียง R73-R78 ค่า 0 Ohm
- ยืนยัน datasheet และ logic threshold ของ EG3112D ว่ารับสัญญาณ 3.3V ที่ HIN/LIN ได้แน่นอน
- เพิ่ม gate-source resistor ประมาณ 10 kOhm ที่ Q1-Q6 เพื่อไม่ให้เกตลอยเมื่อ gate driver ไม่มีไฟ
- C25-C30 ค่า 22 nF เป็นภาระเกตสูง ควรเริ่ม DNP แล้วเลือกค่าจาก waveform, gate charge และ ringing ของ MOSFET จริง
- C18-C20 รวม 6.6 uF ไม่เพียงพอเป็น bulk DC-link สำหรับมอเตอร์ทั่วไป ต้องเพิ่ม bulk/film capacitor, ฟิวส์ และการจำกัดกระแสให้เหมาะสม
- ระบุเบอร์ Q1-Q6 ให้ครบและสอบเทียบ INA181A1 กับกระแสจริงก่อนเพิ่ม current trip หรือพิกัดภาคกำลัง
- software trip ในโค้ดตอบสนองช้าและอาจพลาดกระแสพีก ต้องมี hardware over-current shutdown ก่อนใช้แรงดันหรือกระแสสูง

## หมายเหตุเรื่อง current sense

REF ของ INA181A1 มาจากตัวแบ่ง 1 kOhm/1 kOhm จึงควรอยู่ใกล้ 1.65V หรือประมาณ 2048 counts ที่ ADC 12-bit จาก gain 20 V/V และ shunt 1 mOhm จะได้ประมาณ 24.8 counts/A หรือ 40.3 mA/count โค้ดตั้ง software trip เริ่มต้นประมาณ 2 A และจะไม่ยอม arm หากค่า zero อยู่นอกช่วง 1400-2700 counts
