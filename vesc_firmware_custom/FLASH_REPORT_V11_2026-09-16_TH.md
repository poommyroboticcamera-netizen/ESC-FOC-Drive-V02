# FOC 3-shunt v11 — regular DMA median

แฟลชลง STM32F405 (ID 0x413) ผ่าน ST-LINK สำเร็จ และเครื่องมือรายงาน Download verified successfully ตามด้วย reset

## สิ่งที่แก้

- FOC regular ADC ranks 1–3 อ่าน U/V/W ซ้ำ โดยใช้ median ต่อเฟสใน GET_CURRENT1/2/3 ดังนั้น boot calibration และ FOC feedback ใช้ข้อมูลชนิดเดียวกัน
- Phase voltage ย้ายไป rank 4 (DMA indices 9/10/11); VREFINT ยังอยู่ index 12
- บังคับ FOC V0 sampling และ PWM 25 kHz ก่อน motor init และเมื่อเปลี่ยน configuration
- ADC clock 21 MHz, acquisition 56 cycles: current ranks ใช้เวลาประมาณ 9.71 us และ scan 5 ranks ประมาณ 16.19 us (คำนวณจาก configuration; ยังไม่ได้ยืนยัน timing/settling ด้วย scope)
- Normal ABS threshold 1.5 A / 4 samples คงเดิม; raw emergency 2.2 A / 2 samples ตรวจค่าสูงสุดจากทั้ง 9 raw current conversions แยกจาก median
- เปลี่ยนข้อความ fault จาก BLDC raw เป็น Current magnitude / Current raw peak
- เพิ่ม foc_adc_status สำหรับ regular DMA ranks และ median; foc_hw_status แสดง FOC R/L/flux และ PI
- INA polarity และ pin mapping U/V/W คงตาม v10: CURRENT_CAL = -1; PC0/PC1/PC2

## ตรวจสอบ

- Host tests: median reject one spike / preserve sustained signed current; independent raw emergency despite low median; normal overcurrent sustained trip — PASS
- Firmware build -Wall -Werror — PASS; linker RWX segment warning ของ linker layout เดิมยังมีอยู่
- Flash HEX SHA256: 31DB1E94209B1BF5AC95BCF0679813A272EDF3CCF63D15ABCFEE6F63582B7C1F
- Post-reset software_armed = 0, m_dccal_done = 1
- EEPROM region 0x08004000–0x0800BFFF ต่างจาก preflash backup 0 bytes
- Backup: backups/stm32f405_before_v11_20260916_165045.bin (1 MB), SHA256 5F7163B18E9DDCB871E7D02A14B0F6718A638D7ED0481FE05974664244960E10

## ขอบเขตผล

ยังไม่ได้ทดสอบหมุนมอเตอร์หรือยืนยันว่า noise ลดลงจริง Median ลด isolated conversion spikes แต่ไม่แก้ offset drift, sustained rail/reference noise หรือ motor parameter/polarity ที่ผิด ค่าพารามิเตอร์มอเตอร์ที่เก็บเดิมไม่ได้ถูกแทนด้วยค่าที่เดาใหม่

ขั้นต่อไปอ่าน foc_hw_status และ foc_adc_status ขณะ DISARMED เพื่อเทียบ min/max ของ raw กับ median ก่อนทดสอบขับ
