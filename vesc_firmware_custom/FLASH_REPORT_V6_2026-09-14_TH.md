# รายงานแฟลช BLDC_LITE_24 v6 วันที่ 14 กันยายน 2026

- Target: STM32F405xx, Device ID `0x413`, NVM 1 MB
- ST-LINK target voltage: 3.25 V
- Firmware: `bldc_lite_24_sensorless_sixstep_v6_noise_comp_2026-09-14.hex`
- Firmware SHA-256: `E78E19CC723FE1408F43971F41FC1A1F21381526FC0F287C9B3137D1C8CD855B`
- ผลเขียน: `Download verified successfully`
- รีเซ็ต MCU หลังแฟลชสำเร็จ
- Boot current calibration: DONE (`dccal_done = 1`)
- Software interlock: DISARMED (`software_armed = 0`)
- BLDC zero U/V/W: 2053.457 / 2053.179 / 2047.495 ADC counts
- พื้นที่ configuration 0x08004000–0x0800BFFF เปลี่ยน 0 ไบต์
- ไม่มีการส่งคำสั่งให้มอเตอร์ทำงานระหว่างการแฟลช

## Backup ก่อนแฟลช

- ไฟล์: `backups/stm32f405_before_v6_noise_comp_20260914_full1MB.bin`
- ขนาด: 1,048,576 bytes
- SHA-256: `A89FCE23BEC7BC621B15A0E5F81A5302F8F15881F199127A67E9346834FC674F`
