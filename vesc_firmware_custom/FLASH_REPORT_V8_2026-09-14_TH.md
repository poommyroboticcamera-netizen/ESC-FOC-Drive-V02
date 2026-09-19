# รายงานแฟลช BLDC_LITE_24 v8 วันที่ 14 กันยายน 2026

- Target: STM32F405xx, Device ID `0x413`, NVM 1 MB
- ST-LINK target voltage: 3.25 V
- Firmware: `bldc_lite_24_sensorless_sixstep_v8_magnitude_limits_2026-09-14.hex`
- Firmware SHA-256: `2FD2A628F6F19CBD7A3B966EB3F926D85927605AF92A7009F02B468C5A7410CB`
- ผลเขียน: `Download verified successfully`
- Boot current calibration: DONE (`dccal_done = 1`)
- Software interlock: DISARMED (`software_armed = 0`)
- BLDC zero U/V/W: 2048.931 / 2048.776 / 2042.248 ADC counts
- พื้นที่ configuration 0x08004000–0x0800BFFF เปลี่ยน 0 ไบต์
- ไม่มีการส่งคำสั่งให้มอเตอร์ทำงานระหว่างการแฟลช

## Backup ก่อนแฟลช

- ไฟล์: `backups/stm32f405_before_v8_20260914_full1MB.bin`
- ขนาด: 1,048,576 bytes
- SHA-256: `19688FD9D21BBADE31F1ED4B1BE70CA8B232CC6B3D08BB6510A532A17CC707E3`
