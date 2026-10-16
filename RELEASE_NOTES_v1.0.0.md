# ESC V02 Arduino six-step + SAT motor pilot v1.0.0

วันที่เผยแพร่: 16 ตุลาคม 2026

## ใน release นี้

- `stm32f405-sixstep-current-v1.zip` — open-loop six-step สำหรับ STM32F405 พร้อม INA181 zero calibration และ software current trip
- `stm32f405-sixstep-no-current-v1.zip` — open-loop six-step ที่ไม่อ่าน PC0/PC1/PC2 และไม่มี current trip
- `stm32f405-sixstep-app-v33-proto1.zip` — sensorless six-step พร้อม USB CDC protocol สำหรับ SAT motor pilot
- `SAT-motor-pilot-1.0.0-win-x64.exe` — แอป Windows portable
- `SHA256SUMS.txt` — checksum ของทุกไฟล์ด้านบน

## ความเข้ากันได้

SAT motor pilot รองรับเฉพาะ `fw=v33-proto1`, `protocol=1`, `current_protection=OFF` แอปจะปฏิเสธ firmware current/no-current ที่เป็น Serial Monitor bring-up เพื่อป้องกันการใช้คำสั่งผิดโปรโตคอล

ไม่ได้รวม `SolidGeek/VescUart` เนื่องจากเป็นไลบรารี binary VESC UART protocol ซึ่งไม่ตรงกับ ASCII USB CDC protocol ของ v33

## การเปลี่ยนโครงสร้าง

- รวม Arduino ที่เผยแพร่ไว้ใต้ `arduino_sixstep_releases/`
- เก็บ sketch bring-up รุ่นเก่าทั้งหมดไว้ และเพิ่ม release set ที่ตั้งชื่อ profile ชัดเจนแยกต่างหาก
- เก็บ web app เพียงชุดเดียวใต้ `bldc_tuner_windows/electron-shell/web/`
- ไม่ commit `node_modules`, output ของ Electron/PyInstaller, executable หรือแอป MotorPilot คนละโปรโตคอล

## คำเตือน

ยังไม่มีรุ่นใดผ่านการรับรองกับภาคกำลังจริง รุ่น current ใช้ software trip ที่ตอบสนองช้า รุ่น no-current ไม่มี current feedback และรุ่น app ระบุ current data เป็น `UNVERIFIED` พร้อมปิด current protection ใช้แหล่งจ่ายจำกัดกระแส ฟิวส์ สวิตช์ตัด DC bus และเครื่องมือวัด isolated เสมอ

เครื่องที่จัด release ไม่มี Arduino CLI จึงตรวจ source/protocol แบบ static แต่ยังไม่ได้ Verify ทั้งสาม sketch ด้วย toolchain STM32duino ในรอบนี้ ให้ Verify ใน Arduino IDE ก่อนแฟลช และเริ่มทดสอบโดยถอดมอเตอร์
