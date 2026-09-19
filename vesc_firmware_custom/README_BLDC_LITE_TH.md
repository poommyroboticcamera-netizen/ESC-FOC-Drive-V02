# VESC custom firmware สำหรับบอร์ด SCH_drive_2026-08-24

นี่คือ VESC firmware จากต้นฉบับทางการที่เพิ่ม hardware target ให้ตรงกับบอร์ด STM32F405RGT6 ในสกีแมติก โดยเน้นการทดสอบโต๊ะที่กำลังต่ำ ไม่ใช่เฟิร์มแวร์พร้อมใช้งานกำลังสูงหรือผลิตจริง

## Target ที่ล็อกสำหรับบอร์ดนี้

- VSUPPLY nominal: 24 V
- Target: `fw_bldc_lite_24`
- ไฟล์แฟลช: `output/bldc_lite_24/bldc_lite_24.bin`
- Current amplifier: INA181A1, gain 20 V/V
- Current shunt: 1 mOhm

ที่ ADC 3.3 V จะได้ประมาณ 24.8 ADC counts/A หรือประมาณ 40.3 mA/count ก่อนการสอบเทียบค่าจริง

## การลดทอนเพื่อใช้กับบอร์ดเดิม

- ปิด application อัตโนมัติเป็นค่าเริ่มต้น (`APP_NONE`)
- หลังรีเซ็ตอยู่ในสถานะ `DISARMED` และยอมรับคำสั่งมอเตอร์ไม่ได้
- ต้องสั่ง `bldc_arm CONFIRM` ผ่าน VESC Tool Terminal ทุกครั้งหลังเปิดใหม่
- ก่อน arm ตรวจ SW3/SW4 และ current ADC ทั้งสามว่าต้องอยู่ใกล้ 2048
- กด SW3 หรือ SW4 เพื่อยกเลิกการ arm และสั่ง fault stop
- จำกัด phase current 0.5 A, input current 0.25 A และ duty 5% สำหรับ bring-up
- current-loop PI เริ่มต้น `Kp 0.0005 / Ki 1.0`
- `ABS_OVER_CURRENT` ปกติตัดเมื่อเกิน 1.5 A ต่อเนื่อง 4 รอบควบคุม
- hard cutoff ตัดค่ากระแสดิบทันทีเมื่อเกิน 2.2 A
- ปิดคำสั่ง `foc_openloop_duty`; ใช้เฉพาะ `foc_openloop [current] [erpm]` ในการทดสอบ
- จำกัด regenerative current ใกล้ศูนย์
- HIN/LIN ทั้งหกขาถูกบังคับ LOW ตั้งแต่ early startup ของเฟิร์มแวร์

## Pin mapping หลัก

- High-side PWM: PA8/PA9/PA10 = U/V/W
- Low-side complementary PWM: PB13/PB14/PB15 = U/V/W
- Current ADC: PC0/PC1/PC2
- Phase-voltage ADC: PA0/PA1/PA2
- PPM: PB6
- CAN: PB8/PB9
- USB: PA11/PA12
- SW3/SW4: PC4/PC5 active-low

## ขั้นตอนเปิดครั้งแรก

1. อย่าต่อมอเตอร์ และสำรอง flash เดิมผ่าน ST-LINK ก่อน
2. แฟลชไฟล์ `.bin` ของ target ที่ถูกต้องที่ address `0x08000000`
3. ใช้แหล่งจ่ายจำกัดกระแสและตั้ง current limit ต่ำที่สุดที่ทำให้ภาคควบคุมเปิดได้
4. เชื่อม VESC Tool ผ่าน USB แล้วเปิด Terminal
5. สั่ง `bldc_hw_status`; สถานะต้องเป็น `DISARMED` และ current ADC ทั้งสามควรใกล้ 2048
6. ยังไม่ต้อง arm ให้ตรวจ HIN/LIN และ gate waveform ด้วยออสซิลโลสโคปก่อน
7. เมื่อ current polarity, gain และ dead time ถูกต้องแล้วจึงสั่ง `bldc_arm CONFIRM`
8. หากมีความผิดปกติให้กด SW3/SW4 หรือสั่ง `bldc_disarm`

## ข้อจำกัดที่แก้ด้วย firmware ไม่ได้

- ไม่มี hardware over-current comparator และไม่มีสัญญาณ fault จาก gate driver เข้า MCU
- ไม่มี MOSFET temperature sensor
- ไม่มี VSUPPLY ADC; ค่าแรงดันในเฟิร์มแวร์เป็น nominal/phase estimator ไม่ใช่ over-voltage protection
- HIN/LIN ยังเป็น high-impedance ระหว่าง power-on/reset ก่อนโค้ดแรกเริ่มทำงาน
- U13-U15 ยืนยันเป็น INA181A1 gain 20 V/V แล้ว แต่ยังต้องสอบเทียบ offset/gain จากบอร์ดจริง
- dead time 1000 ns เป็นค่าเริ่มต้นที่ต้องยืนยันจาก waveform จริง
- DC-link capacitance และ gate capacitance บนบอร์ดเป็นข้อจำกัดทางฮาร์ดแวร์ที่เฟิร์มแวร์ชดเชยไม่ได้

ห้ามใช้ Motor Detection อัตโนมัติหรือเพิ่ม current/duty จนกว่าจะสอบเทียบ current amplifier และตรวจ shoot-through/dead time ด้วยเครื่องมือวัดจริง

ชุดป้องกันกระแสไม่ได้ถูกถอดออก: spike เพียงหนึ่ง sample จะถูกละเว้น แต่กระแสที่เกิน 1.5 A ต่อเนื่องหรือค่าดิบเกิน 2.2 A ยังสั่งหยุดทันทีตามเงื่อนไขข้างต้น

รายละเอียด source อยู่ที่ `hwconf/custom_bldc/README_TH.md` และผลการบิลด์อยู่ที่ `BUILD_INFO_TH.md`
