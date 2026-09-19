# VESC-lite custom target สำหรับ V.Phase2_v02

โฟลเดอร์นี้เป็น hardware configuration ของ VESC firmware สำหรับวงจร `SCH_drive_2026-08-24.pdf` โดยไม่แก้ PCB เป้าหมายปัจจุบันคือทดสอบ Sensorless BLDC six-step ที่แรงดันและกระแสต่ำ ไม่ใช่เฟิร์มแวร์พร้อมใช้งานกำลังสูง

## Target ที่ยืนยันแล้ว

- `fw_bldc_lite_24` สำหรับ VSUPPLY nominal 24 V
- current amplifier U13-U15 คือ INA181A1, gain 20 V/V

target 48 V ถูกถอดออกเพื่อป้องกันการเลือกไฟล์ผิด เพราะวงจรไม่มี ADC วัด VSUPPLY โดยตรง

## สิ่งที่พอร์ตแล้ว

- TIM1: PA8/PA9/PA10 เป็น UH/VH/WH และ PB13/PB14/PB15 เป็น UL/VL/WL
- ADC current: PC0/PC1/PC2
- ADC phase voltage: PA0/PA1/PA2
- current polarity ของบิลด์ BLDC เป็น `NORMAL`: ตามสกีแมติก INA181 ต่อ IN+ ที่ AGND และ IN- ที่ด้าน MOSFET ของ low-side shunt; โค้ด six-step จะกลับเครื่องหมายของช่อง low-side ที่กำลังนำกระแสให้อยู่แล้ว
- motor type ถูกบังคับเป็น sensorless BLDC six-step ก่อนเริ่ม motor-control แม้ EEPROM จะยังเก็บค่า FOC เดิม
- BLDC ใช้ค่า VSUPPLY อ้างอิงคงที่ 24 V เพราะบอร์ดไม่มี physical VSUPPLY ADC; ห้ามเปลี่ยนแรงดันโดยไม่สร้าง firmware ให้ตรงกัน
- shunt 1 mOhm และ INA181A1 gain 20 V/V
- CAN1 PB8/PB9, USB PA11/PA12 และ PPM PB6
- SW3/SW4 เป็น software stop polling 1 ms
- บังคับ HIN/LIN เป็น LOW ตั้งแต่ `HW_EARLY_INIT`
- PWM switching เริ่มต้นประมาณ 15 kHz (`f_zv` 30 kHz), dead time เริ่มต้น 1000 ns
- จำกัด phase current 0.5 A, input current 0.25 A และ duty 5%
- current-loop PI สำหรับ bring-up คือ `Kp 0.0005 / Ki 1.0`
- ตัด `ABS_OVER_CURRENT` เมื่อค่าดิบเกิน 1.5 A ต่อเนื่อง 4 รอบ หรือเกิน 2.2 A เพียงรอบเดียว
- กรองขนาดกระแสหลังคำนวณ magnitude เพื่อไม่ให้กระแสสลับบวก/ลบหักล้างกัน
- ปิด terminal command `foc_openloop_duty` เพื่อไม่ให้ป้อนแรงดันตรงตอนมอเตอร์ความเร็วต่ำ
- จำกัด negative current/regen ใกล้ศูนย์
- software interlock: หลังเปิดใหม่ทุกครั้งต้องสั่ง `bldc_arm CONFIRM`
- SW3/SW4 จะยกเลิกการ arm และสั่ง fault stop ทันที

## สิ่งที่ firmware ชดเชยไม่ได้

- ไม่มี VSUPPLY ADC: firmware ใช้ค่า nominal และ phase-peak estimator ซึ่งไม่ใช่ over-voltage protection
- ไม่มี gate enable, driver fault หรือ hardware over-current comparator
- ไม่มี MOSFET temperature sensor
- U13-U15 ยืนยันเป็น INA181A1 gain 20 V/V แต่ยังต้องสอบเทียบ offset/gain ของบอร์ดจริง
- ขา HIN/LIN ยัง high-impedance ช่วง reset ก่อนเฟิร์มแวร์เริ่มทำงาน
- C18-C20 รวมเพียง 6.6 uF และ C25-C30 ที่ gate มีค่า 22 nF

## Build

จาก root ของ VESC firmware:

```text
make arm_sdk_install
make fw_bldc_lite_24
```

ผลลัพธ์อยู่ที่:

```text
build/bldc_lite_24/bldc_lite_24.bin
```

## ก่อนแฟลช

1. วัดว่าขา current amplifier ทั้งสามอยู่ใกล้ 1.65 V ขณะไม่มีกระแส
2. ตรวจ logic ด้วยสเก็ตช์ Arduino ที่ทำไว้ก่อน และวัดว่า HIN/LIN เฟสเดียวกันไม่ HIGH พร้อมกัน
3. ใช้ ST-LINK ที่ H19 และเก็บสำเนา flash เดิม
4. ยังไม่ต่อมอเตอร์ในการเปิด VESC firmware ครั้งแรก
5. เปิด VESC Tool ผ่าน USB แล้วรัน terminal command `bldc_hw_status`
6. ตรวจ current ADC ควรอยู่ใกล้ 2048 และ phase ADC ควรใกล้ศูนย์เมื่อไม่จ่ายภาคกำลัง
7. เมื่อค่าถูกต้องจึงสั่ง `bldc_arm CONFIRM`; การ arm มีผลเฉพาะ boot ปัจจุบัน
8. ทดสอบ PWM/gate waveform ด้วยแหล่งจ่ายจำกัดกระแสและ differential probe ก่อนต่อมอเตอร์

ห้ามใช้ Motor Detection อัตโนมัติจนกว่าจะยืนยัน current polarity, gain, dead time และ gate waveform ด้วยเครื่องมือวัดจริง
