# รายงานแฟลช BLDC_LITE_24 v7 วันที่ 14 กันยายน 2026

- Target: STM32F405xx, Device ID `0x413`, NVM 1 MB
- ST-LINK target voltage: 3.25 V
- Firmware: `bldc_lite_24_sensorless_sixstep_v7_magnitude_feedback_2026-09-14.hex`
- Firmware SHA-256: `F54D549F08AEB38580114B70BFFA55D19D66891D559265E3C2EB28FE81D2CC2F`
- ผลเขียน: `Download verified successfully`
- Boot current calibration: DONE (`dccal_done = 1`)
- Software interlock: DISARMED (`software_armed = 0`)
- BLDC zero U/V/W: 2048.812 / 2048.640 / 2042.102 ADC counts
- พื้นที่ configuration 0x08004000–0x0800BFFF เปลี่ยน 0 ไบต์
- ไม่มีการส่งคำสั่งให้มอเตอร์ทำงานระหว่างการแฟลช

## Backup ก่อนแฟลช

- ไฟล์: `backups/stm32f405_before_v7_20260914_full1MB.bin`
- ขนาด: 1,048,576 bytes
- SHA-256: `B36EF0C3123F76D0E13C3FEF5B9DFC7484D4B498515999D4393244AFAE49F18C`
