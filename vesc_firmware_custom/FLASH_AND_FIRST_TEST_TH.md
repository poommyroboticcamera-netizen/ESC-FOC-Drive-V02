# การสำรองและแฟลช BLDC_LITE_24

เครื่องนี้มี STM32CubeProgrammer 2.22.0 แล้ว แต่ ณ เวลาจัดทำคู่มือนี้ยังไม่พบ ST-LINK เชื่อมต่ออยู่ จึงยังไม่มีการเขียนข้อมูลลงบอร์ด

## การต่อ ST-LINK

ต่อจาก ST-LINK ไปยังหัว SWD ของบอร์ด:

- SWDIO
- SWCLK
- GND
- NRST ถ้ามีบนหัวต่อ
- VTref/3.3V sense ตามชนิด ST-LINK

อย่าใช้ขา 3.3V ของ ST-LINK จ่ายภาคกำลัง และอย่าต่อ VSUPPLY/มอเตอร์จนกว่าจะทราบวิธีจ่ายไฟให้ target อย่างปลอดภัย

## 1. ตรวจว่าพบ ST-LINK

เปิด PowerShell ที่ root ของ VESC firmware แล้วรัน:

```powershell
.\tools\flash_bldc_lite_24.ps1 -Probe
```

คำสั่งนี้ไม่อ่านหรือเขียน flash

## 2. สำรอง flash เดิม

```powershell
.\tools\flash_bldc_lite_24.ps1 -BackupOnly
```

สคริปต์จะอ่าน internal flash 1 MiB จาก `0x08000000` และบันทึกไว้ใน `backups` พร้อมแสดง SHA-256 หากสำรองไม่สำเร็จสคริปต์จะหยุดและไม่แฟลช

สคริปต์จะไม่สั่ง read-unprotect เพราะการปลด RDP อาจลบข้อมูลเดิม

## 3. แฟลชเฟิร์มแวร์ 24 V

ทำเฉพาะเมื่อ:

- ถอดมอเตอร์แล้ว
- แหล่งจ่ายถูกถอดหรือจำกัดกระแสอย่างเหมาะสม
- ต่อ GND/SWD/NRST ถูกต้อง
- สำรอง flash เดิมได้สำเร็จ

จากนั้นรัน:

```powershell
.\tools\flash_bldc_lite_24.ps1 -Flash24V -MotorDisconnected -PowerLimited
```

ก่อนเขียน สคริปต์จะสำรอง flash อีกครั้งและตรวจว่า SHA-256 ของ firmware ตรงกับค่าที่บิลด์ไว้ จากนั้นจึงเขียนที่ `0x08000000`, verify และ reset

## 4. หลังแฟลช

1. ยังไม่ต่อมอเตอร์
2. เชื่อม USB และเปิด VESC Tool
3. เปิด Terminal แล้วสั่ง `bldc_hw_status`
4. ต้องเห็น `BLDC_LITE_24`, `INA181A1` และ `Software interlock: DISARMED`
5. ต้องเห็น PI `0.0005 / 1.00`, ABS protection `1.50 A for 4 samples` และ raw emergency `2.20 A`
6. ต้องเห็น `Open-loop duty terminal command: DISABLED`
7. Current ADC U/V/W ควรใกล้ 2048 และใกล้กันทั้งสามช่อง
8. ตรวจ HIN/LIN และ gate waveform ก่อน
9. เมื่อผ่านทั้งหมดจึงสั่ง `bldc_arm CONFIRM`; การ arm มีผลเฉพาะ boot ปัจจุบัน

ห้ามเริ่ม Motor Detection ในขั้นนี้

เมื่อผ่านการตรวจโดยไม่ต่อมอเตอร์แล้ว จึงต่อมอเตอร์และใช้ชุดทดสอบกระแสที่ส่งคำสั่ง heartbeat ให้อัตโนมัติ:

```powershell
.\tools\test_bldc_openloop_current_usb.ps1 -Port COM7 -Current 0.20 -Erpm 100 -DurationMs 800
```

อย่าใช้ `foc_openloop_duty` กับ target นี้ เนื่องจากเป็นการกำหนดแรงดันโดยตรงและถูกปิดไว้ในเฟิร์มแวร์
