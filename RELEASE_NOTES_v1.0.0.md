# SAT motor pilot v1.0.0 — Windows app

ไฟล์ใน Release นี้: `SAT-motor-pilot-1.0.0-win-x64.exe` (Windows x64 portable)

SHA-256: `35af8c3fc4afd5b111897b246f90dde943830b38c81b85e6e61734e9b7fa18d8`

ใช้คู่กับ [Arduino six-step app firmware v33-proto1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-app-v33-proto1) เท่านั้น แอปตรวจ `fw=v33-proto1`, `protocol=1` และ `current_protection=OFF` ก่อนเปิดการควบคุม จึงไม่รองรับ firmware current/no-current ที่สั่งผ่าน Serial Monitor

วิธีติดตั้งและใช้งาน: [README หลัก](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02#เริ่มใช้งาน-sat-motor-pilot) และ [คู่มือแอป](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/bldc_tuner_windows/README_TH.md)

> ยังไม่ผ่านการทดสอบกับภาคกำลังจริง กระแสเป็นข้อมูล diagnostic ที่ยังไม่ยืนยัน และ current protection ของ firmware ที่รองรับแอปถูกปิด ใช้ฟิวส์ แหล่งจ่ายจำกัดกระแส และสวิตช์ตัด DC bus เสมอ
