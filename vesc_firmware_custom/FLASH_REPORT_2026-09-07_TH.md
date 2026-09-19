# รายงานแฟลช VESC custom v5 วันที่ 7 กันยายน 2026

แฟลชตามคำขอผู้ใช้เพื่อเตรียมทดสอบ ใช้รุ่น v5 เดิมสำหรับ BLDC_LITE_24 / INA181A1 ไม่ได้เพิ่มการแก้ noise ในรอบนี้

## ไฟล์และการสำรอง

- ไฟล์: `output/bldc_lite_24/bldc_lite_24_sensorless_sixstep_v5_no_startup_boost_raw_oc_2026-09-03.hex`
- HEX SHA-256: `CF5DD4379F75BB815FAF4ED9674871B1FF9CF30D0D4C1D145A190E87ABA61F84`
- BIN คู่กัน SHA-256: `F421E42605F954193C6484A84ACA99AE7EF0C0575343F754A66175AC259EE277`
- สำรอง flash ทั้งหมด 1 MB ก่อนแฟลช: `backups/stm32f405_before_reflash_v5_20260907_113805_full1MB.bin`
- Backup SHA-256: `3F19F6BD9ED0B61255EACC51E0F0127BDC2C424DC0AC26AC9A4BCE06828AC70C`

ตรวจ checksum ของ Intel HEX และเทียบข้อมูลทั้งหมด 454,348 ไบต์กับ BIN แล้ว ขอบเขตการเขียนอยู่เฉพาะ sector 0 และ 3–7 จึงเก็บ EEPROM sector 1–2 ไว้

## ผลแฟลชและบูต

- ST-LINK, SWD 4 MHz, device ID 0x413, target voltage 3.25 V
- เขียนผ่าน STM32CubeProgrammer และได้รับ `Download verified successfully`
- หลังรีเซ็ต USB COM7 ตอบ `Hardware: BLDC_LITE_24`, BLDC sensorless six-step และ startup boost 0.0000
- `Current DC calibration: DONE`, `Software interlock: DISARMED`
- อ่าน RAM หลังบูตยืนยัน offset U/V/W = 2050 / 2050 / 2043 counts, `dccal_done = 1`, `software_armed = 0`
- อ่าน EEPROM 32 KB กลับเทียบกับก่อนแฟลช: ต่าง 0 ไบต์
- ขีดจำกัด phase/input/ABS = 0.50/0.25/1.50 A, duty สูงสุด 0.050; ABS ต่อเนื่อง 4 samples และเกณฑ์ฉุกเฉิน 2.20 A
- ไม่มีคำสั่ง ARM หรือขับมอเตอร์ในรอบแฟลชนี้

## ข้อจำกัดที่ยังมี

หลังแฟลชคำสั่ง `bldc_injected_status` จำนวน 128 ตัวอย่างรายงาน median U = 2049 (2045–2058), V = 2049 (2043–2066), W = 2043 (2036–2062) counts จึงยังมีความแกว่งเมื่อ DISARMED การแฟลชและคาลิเบรตสำเร็จไม่ยืนยันว่า ABS_OVER_CURRENT ขณะขับมอเตอร์ได้รับการแก้แล้ว

DRV ที่บันทึกขณะ DISARMED เกิดในบริบท software interlock ของพอร์ตนี้ ซึ่งไม่มีขา driver fault ทางกายภาพเชื่อมต่อ ค่าบัส 24 V ที่รายงานเป็นค่า nominal/estimate ไม่ใช่การยืนยันแรงดันแหล่งจ่ายจริง
