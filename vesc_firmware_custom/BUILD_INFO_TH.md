# Build information

- วันที่บิลด์: 2026-09-02
- VESC firmware upstream commit: `4c111e2c5dcd2108a2c1029181940aa7f6d04dbd`
- ARM GNU Toolchain: `14.2.1` (xPack จาก STM32 Arduino package)
- MCU target: STM32F405RGT6
- Board target ที่ยืนยัน: VSUPPLY nominal 24 V
- Current amplifier: INA181A1, gain 20 V/V
- Current shunt: 1 mOhm
- สกีแมติก SHA-256: `114434969101F552F082A9FF0F430AABB4DDFE5F8C6B2A9A625C2F5A53F458A5`

## ผลการคอมไพล์

| Target | ผล | text | data | bss |
|---|---:|---:|---:|---:|
| `fw_bldc_lite_24` | ผ่าน | 449556 | 2812 | 172864 |

target 24 V สร้าง `.bin`, `.hex` และ `.elf` สำเร็จ มีคำเตือนจาก linker ว่า ELF มี LOAD segment แบบ RWX แต่ไม่มี compile/link error

## SHA-256 ของผลลัพธ์

```text
6F7BBE201B1503A35685F26281C14176CCA09C1FAB848BB63421453B01FF78ED  output/bldc_lite_24/bldc_lite_24.bin
F69EE18519CEFEA0B043502CE5F854A59E1D6B2FC658F2EF4C39F84874B7E978  output/bldc_lite_24/bldc_lite_24.hex
692BE3E227AB2CE9DC909701513ABF1E973AAE1200ADCE046BA92076EF8751AC  output/bldc_lite_24/bldc_lite_24.elf
```

## สิ่งที่ตรวจแล้ว

- vector table ของ `.bin` มี stack pointer ใน RAM และ reset vector แบบ Thumb ที่ถูกต้อง
- ELF มีฟังก์ชัน custom early gate-low, ADC setup, bus estimator, stop-button thread และ software arm/disarm
- binary มี target name `BLDC_LITE_24` และข้อมูล current sense `INA181A1`
- pin mapping ใน source ตรงกับ TIM1/ADC/SW จากสกีแมติก
- current-loop PI ถูกลดเป็น `0.0005 / 1.0`
- normal over-current ใช้ค่าขนาดกระแสดิบเกิน 1.5 A ต่อเนื่อง 4 รอบ และมี emergency cutoff 2.2 A ทันที
- `i_abs_filter` กรอง magnitude โดยตรง จึงไม่ซ่อนกระแสที่สลับบวก/ลบ
- terminal command `foc_openloop_duty` ถูกปิดสำหรับ target นี้
- PowerShell test ส่ง current command ซ้ำก่อนครบ timeout และหยุดเมื่อ feedback เกินกรอบทดสอบ
- Arduino STM32 test sketch คอมไพล์ผ่าน: flash 44008 bytes, RAM 5256 bytes

ผลนี้ยืนยันการคอมไพล์และ static verification ของ revision นี้ ยังไม่ได้แฟลชหรือทดสอบ revision นี้กับฮาร์ดแวร์จริง
