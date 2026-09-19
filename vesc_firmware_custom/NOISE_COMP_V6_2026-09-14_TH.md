# BLDC_LITE_24 noise compensation v6

รุ่นนี้เตรียมไว้สำหรับ INA181A1, gain 20 V/V, shunt 1 mOhm และระบบ BLDC sensorless six-step 24 V

## การเปลี่ยนแปลง

- ตั้งศูนย์กระแสตอนบูตจาก 4000 ตัวอย่างแบบทศนิยม ไม่ตัดเศษ ADC count
- หยุดสะสมค่าตั้งศูนย์หลังครบ 4000 ตัวอย่าง เพื่อไม่ให้ตัวแปรล้นและไม่ปรับศูนย์ขณะจ่าย PWM
- เพิ่ม RC ดิจิทัล tau 50 us เฉพาะ feedback ของ current controller
- คงค่า trip ปกติ 1.50 A จำนวน 4 ตัวอย่างติดกัน
- คงค่า emergency 2.20 A แต่ต้องพบ 2 ตัวอย่างติดกัน จึงไม่ตัดจาก spike เดี่ยว
- timeout, software arm/disarm และข้อจำกัด duty 0.050 ยังคงทำงาน
- `bldc_hw_status` แสดงค่า zero U/V/W และรายละเอียด noise compensation

## การตรวจสอบ

- host unit test ผ่าน: step response, DC convergence และ alternating noise
- host unit test ผ่าน: spike เดี่ยวไม่ trip, กระแสเกินต่อเนื่องยัง trip
- full rebuild ด้วย `-Wall -Werror` ผ่าน
- ขนาด ELF: text 451896, data 2816, bss 172880 bytes
- linker มีคำเตือน LOAD segment RWX เดิมของ linker script แต่ไม่มี compile/link error

## SHA-256

```text
E78E19CC723FE1408F43971F41FC1A1F21381526FC0F287C9B3137D1C8CD855B  HEX
B3906E8EDF1B9F00AD6FFCC2FCE4613DEBF3BC19DCDF7B39769336A7D562F696  BIN
407FC969BB168A7F9F676FACFA4EB89BBB700DE2E1E7B1B34C2CD4839C36A481  ELF
```

รุ่นนี้ผ่านการทดสอบซอฟต์แวร์ แต่ยังต้องแฟลชและทดสอบบนบอร์ดจริงด้วยพัลส์ 0.05 A ก่อนอนุญาตให้ทำงานต่อเนื่อง ห้ามเพิ่ม threshold หากยังพบ ABS_OVER_CURRENT ต่อเนื่อง เพราะอาจเป็นกระแสจริงหรือ ringing ที่ฮาร์ดแวร์
