# Arduino six-step current v1

ไฟล์ใน Release นี้: `stm32f405-sixstep-current-v1.zip` สำหรับ STM32F405

SHA-256: `41c9ef685e92431f3b0fc208826f3bf9d339aa6511d98fcc7d4af30b0b86b997`

รุ่น bring-up ผ่าน Serial Monitor ใช้ INA181 บน PC0/PC1/PC2, zero calibration และ software current trip **ไม่รองรับ SAT motor pilot** และ software trip ไม่ทดแทนวงจร hardware over-current shutdown

วิธีแฟลชและใช้งาน: [คู่มือ Arduino six-step](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/README_TH.md)

> ค่ากระแสต้องสอบเทียบกับวงจรจริงก่อนเชื่อผล รุ่นนี้ยังไม่ผ่านการรับรองกับภาคกำลังจริง ใช้แหล่งจ่ายจำกัดกระแส ฟิวส์ และสวิตช์ตัด DC bus
