# MKSESC 75100 V1 → CUSTOM BOARD PORTING REPORT

วันที่ตรวจ: 2026-09-07. สถานะ: ตรวจ schematic, Gerber 4 ชั้น, drill และ FlyingProbe netlist แล้ว; **ยังไม่ผ่าน as-built electrical/thermal validation และยังไม่พร้อมเปิด power stage**.

## ขอบเขตและหลักฐาน

- คำขอ: pasted-text.txt ที่ผู้ใช้ระบุว่าเป็นคำขอโดยตรง; ข้อความใน schematic เป็นหลักฐานของวงจร ไม่ใช่คำสั่งให้ดำเนินการอื่น
- Custom: `C:/Users/poomc/Downloads/SCH_drive_to_compare_2026-09-04.pdf`, 5 หน้า, ชื่อบอร์ดใน title block `V.Redesign supply`; อ่านและตรวจภาพทุกหน้าแล้ว
- Reference: [Makerbase schematic V1.0, commit 6bb0487](https://github.com/makerbase-mks/VESC-MKS/blob/6bb048761b5f1f5c83ce0b1d86e02922cb5fdafb/04_Hardware/MKSESC%2075100%20V1.0/MKSESC%2075100_001-schematic.pdf). สำเนาอยู่ที่ `evidence/MKSESC_75100_V1_schematic.pdf`
- Firmware ที่มีอยู่จริง: repository `vedderb/bldc`, HEAD `4c111e2c5dcd2108a2c1029181940aa7f6d04dbd`, มี `hwconf/makerbase/75_100/hw_mksesc_75_100_old.h` และการแก้ source เดิมของผู้ใช้ ต้องไม่เรียกว่าเป็น source snapshot ของ Makerbase repository
- ผู้ใช้ยืนยันในบทสนทนา: current amplifier = **INA181A1**, MOSFET แก้ไขชื่อเป็น **IRFS7530**. Gerber ใช้ footprint 7 pins จึงสอดคล้องกับตระกูล IRFS7530-7P; รอยืนยัน suffix เต็มก่อนใช้ Qg/thermal model เฉพาะรุ่น
- ผู้ใช้แจ้งว่า **ถอด C 100 nF ที่ output แล้ว**. PDF ยังแสดง C39/C42/C45 จึงบันทึกเป็น as-built modification; รอยืนยันว่าครบทั้งสามช่อง
- ได้รับ `C:/Users/poomc/Downloads/Gerber_V02_2026-09-07.zip` ระหว่างทำงาน มี FlyingProbeTesting.json, copper4ชั้น และ drill3ไฟล์. ยังไม่มี native CAD/ERC, BOM ที่ผูก part number เต็ม, copper stack-up หรือผลวัดบอร์ด revision นี้. หน้า5ของPDFเป็นภาพราคา/3D ไม่ใช้แทนGerber

## ผลสำคัญก่อน port

1. MCU, PWM pins, current ADC pins และ topology 3 low-side shunts สอดคล้อง reference แต่ไม่ใช่เหตุผลให้ใช้ configuration เดิมทั้งชุด
2. Shunt เพิ่มจาก 0.5 mΩ สามตัวขนานต่อเฟส (0.166667 mΩ effective) เป็น 1 mΩ ต่อเฟส: sensitivity เพิ่ม **6 เท่า** เมื่อ gain เท่ากัน และช่วง ADC ลดเหลือประมาณ ±82.5 A ที่ 3.3 V แบบอุดมคติ ไม่ใช่พิกัดกระแสใช้งานบอร์ด
3. **ไม่มี direct VBUS divider เข้า MCU ใน PDF**: PC3 pin 11 ว่าง; divider 56k/2.2k ที่ PA0/1/2 เป็น phase voltage. จึงยังทำ bus UV/OV และ regen protection ตามคำขอไม่ได้
4. **ไม่มี FET/motor temperature measurement ใน PDF**. PA3 ว่าง, PC4/PC5 เป็นปุ่มกด ห้ามนำ ADC ช่องเหล่านี้ไปตีความเป็น NTC
5. ไม่พบ gate-enable, driver-fault pin หรือ comparator-to-TIM1_BKIN path; PB12 ถูกทำเครื่องหมาย NC. Software shutdown มีได้ แต่ไม่ใช่ hardware over-current shutdown
6. ไม่มี switched phase-filter circuitry; มีเพียง phase divider. Flag ใหม่ต้องเป็น `HW_HAS_PHASE_FILTER = 0`
7. `3.3VDD` ที่ MCU แยกจาก output regulator `3.3V` ทั้งใน FlyingProbe netlist และการตรวจ copper แบบ raster20µm ไม่พบทางเชื่อม. ต้องยืนยันจัมเปอร์/การแก้จริงก่อน boot. ส่วน driver GND เป็น AGND ในexport และตรวจพบทาง copper ถึง MCU ground

## PHASE 1 — MKSESC 75100 V1 Reference Analysis

### MCU / power stage

Reference U2 = STM32F405RGT6, LQFP64, S1 = 8 MHz. Firmware clock branch สำหรับ HSE 8 MHz ใช้ PLL M=8, N=336, P=2, Q=7: SYSCLK 168 MHz, APB1 42 MHz, APB2 84 MHz, TIM1 168 MHz, TIM2 84 MHz, USB 48 MHz. Clock เป็น configuration ของ software ไม่ใช่ค่าที่พิสูจน์จาก oscillator symbol เพียงอย่างเดียว

J2–J7 = MDP10N27, หก MOSFET symbols, หนึ่ง high + หนึ่ง low ต่อเฟส (สองต่อเฟส รวมสาม half bridges). U6/U7/U8 = EG3112; phase A ใช้ U7, B ใช้ U8, C ใช้ U6. Driver supply ระบุ +12 V, มี FR107 bootstrap diode และ C bootstrap 10 µF. Gate resistance 10 Ω ทั้งหกช่อง; มี external gate-source 22 nF ทั้งหกตัว ซึ่งเพิ่ม gate charge และต้องรวมใน timing budget

DC link ที่แสดง: C57/58/59 = 2.2 µF ต่อ bus และ C11 2.2 µF/C46 100 nF ที่ buck input; ไม่พบ bulk electrolytic bank ขนาดใหญ่ใน schematic นี้ จึงไม่รับรองว่าบอร์ดผลิตจริงไม่มี capacitor เพิ่ม. D4 = SMCJ85A บน bus. 22 nF gate-source ไม่ใช่ drain-source RC snubber; ไม่พบ dedicated RC snubber ใน reference sheet

Current: U9/U10/U11 = INA181A1, gain 20. R48/R49/R50 ระบุ `0.5MR*3`, สอดคล้อง header `(0.0005/3.0)` แต่ยังเป็น schematic interpretation ไม่ใช่การวัดบอร์ดผลิตจริง. IN+ ลง GND, IN− ขึ้น low-side source; REF divider 1k/1k ให้ครึ่ง rail, output shunt cap 1 nF, input differential cap 100 nF

Supplies: SCT2A23ASTER → 12 V, RY8120 → 5 V/3.3 V. Rail TVS SMF12A/SMF5.0A/SMF3.3A. Temperature T1 ตำแหน่ง NTC ถูกระบุ NC จึง **ไม่มี R25/B ที่ยืนยันจาก schematic** แม้ firmware จะสมมติ 10k B3435. Hall connector CN2, UART CN1/CN3, USB CN7, CAN MCP2551 CN4 และ PPM CN6 แสดงในวงจร

### ความขัดแย้ง schematic กับ reference header

| รายการ | Schematic V1 | `hw_mksesc_75_100_core.h` | ผลต่อ port |
|---|---|---|---|
| VBUS divider | R12 560k / R10 21.5k | 56k / 2.2k; คอมเมนต์พูด 560k/21.5k | ต้องแยก bus scale จาก phase scale |
| Phase divider | 56k / 2.2k | ใช้ VIN constants ร่วมกันบางเส้นทาง | ไม่ถือ bus/phase ratio เท่ากัน |
| Analog supply | label 3.3 V | V_REG 3.34 | วัด VDDA และ calibrate |
| FET NTC | T1 NC, ไม่ระบุ beta | 10k B3435 | default ไม่ยืนยันชิ้นส่วน |
| Phase filtering | ไม่มี circuit | `HW75_100_OLD` ไม่เปิด plural flag | อย่าเลือก target รุ่นใหม่ที่เปิด filter |
| Switching parameter | schematic ไม่กำหนด | `foc_f_zv=30000` | source ตั้ง ARR=168MHz/f_zv; carrier center-aligned ≈15 kHz ไม่ใช่ 30 kHz |
| Dead-time | ต้องวัดจริง | 660 ns | ไม่ถ่ายค่ามาเป็นค่าผ่านการทดสอบของ custom |
| Limits | ต้องอาศัย ratings/layout | header มี voltage/current bounds หลายชุด | bounds ของ UI/firmware ไม่ใช่ absolute hardware ratings |

## PHASE 2 — Custom Hardware Analysis

หน้า 1: R20/R39/R40 เป็น designator ของ EG3112D แม้ขึ้นต้น R; จับคู่ U/V/W ตาม net. Bootstrap D4–D6 FR107, C31–C33 10 µF; local supply C34–C36 10 µF. Q1/Q2 = U, Q3/Q4 = V, Q5/Q6 = W. C18–C20 รวม nominal 6.6 µF บน bus. Gate R41/42/44/43/45/46 = 10 Ω; gate-source C30/25/29/26/28/27 = 22 nF. ไม่พบ bus TVS หรือ dedicated drain-source snubber บนหน้า 1–4. ไม่ทราบ voltage rating, DC-bias derating หรือ ripple-current rating ของ capacitor

หน้า 2: U18 = STM32F405RGT6; X4 = **8 MHz** (ข้อความ extraction `X48MHz` เป็น designator X4 ติดกับค่า 8MHz), C77/C78 = 18 pF, VCAP C79/C80 = 2.2 µF. PWM ผ่าน zero-ohm R73–R78. Phase divider ผ่าน R79–R81. PC4 และ PC5 เป็น active-low buttons พร้อม 10k/100nF. LED1 เป็น power LED ต่อกับ rail ไม่ใช่ GPIO. USB และ CAN มี connector จริง; UART/Hall/encoder pins ไม่มี connector ใน PDF นี้

หน้า 3: U21 SCT2A23STER, feedback 274k/30k, rail label 12 V. U23/U25 มีแต่ pin diagram ไม่มี part value จึงยังยืนยันว่าเป็น RY8120 ไม่ได้ แม้ topology คล้าย reference. U30 TPS54331D วางแยกและไม่มีเส้นต่อ **ไม่ใช่ regulator ที่ใช้งานในวงจรตาม PDF**. 5 V feedback ใช้ 73.2k และ 4.3k อนุกรมด้านบน /10k ด้านล่าง. หากใช้ regulator VFB=0.6 V จะได้ 5.25 V; 3.3 V = 0.6×(1+45.3/10)=3.318 V. นี่เป็นการคำนวณมีเงื่อนไข ต้องยืนยัน U23/U25 และวัด rail

หน้า 4: R47/R50/R53 = 1 mΩ low-side; U13/U14/U15 ผู้ใช้ยืนยัน INA181A1. C37/C40/C43 = 100 nF คร่อม shunt; C38/C41/C44 = 100 nF supply bypass. **แยกจาก C39/C42/C45 ที่ output** ซึ่งผู้ใช้แจ้งว่าถอดแล้ว. Datasheet INA181 ระบุ capacitive-load condition 1 nF; 100 nF ต่อตรง output ตาม PDF เกินเงื่อนไขนั้นมาก. หลังถอดต้องวัด settling/noise ใหม่ ไม่ถือว่า timing ผ่านโดยอัตโนมัติ

MOSFET: ผู้ใช้ยืนยัน **IRFS7530**, VDS60V/VGS±20V ตาม[Infineon](https://www.infineon.com/part/IRFS7530). Gerberแสดง Q1–Q6 pin1=gate,pin4=drain/tab,pin2/3/5/6/7=source ซึ่งสอดคล้องกับ [IRFS7530-7P](https://www.infineon.com/part/IRFS7530-7P). รุ่น3ขากับ7ขามีQg/RDS(on)ต่างกัน; ให้ยืนยันsuffixก่อนgate-loss calculation. ทั้งสองไม่ใช่MOSFETสำหรับbus75V. 60Vเป็นabsoluteVDS ceiling ต้องเผื่อovershoot ไม่ตั้งbus_limit=60V

## PHASE 3 — Hardware Diff Table

SAME หมายถึงข้อมูล schematic ที่เทียบตรงกัน; PARTIAL ไม่ใช่รับรอง PCB/การประกอบจริง; UNKNOWN ต้องมีหลักฐานเพิ่ม

| FUNCTION | MKSESC 75100 V1 | CUSTOM BOARD | SAME / DIFFERENT | FIRMWARE IMPACT | ACTION REQUIRED |
|---|---|---|---|---|---|
| MCU/package | STM32F405RGT6 LQFP64 | U18 รุ่นเดียวกัน | SAME | ใช้ Cortex-M4F/STM32F4 platform ได้ | ยืนยัน marking/SWD ID |
| HSE | 8MHz S1 | 8MHz X4 +18pF | PARTIAL | clock plan เดิมเป็น candidate | วัด MCO/ตรวจ crystal spec |
| Power rails | SCT2A23 + RY8120×2 | SCT2A23STER + U23/U25 ไม่ระบุ | UNKNOWN | boot, BOR, ADC reference | ยืนยันรุ่น/วัด 12/5/3.3/VDDA |
| Ground / MCU rail | net เชื่อมใน reference | AGNDเชื่อม; 3.3VDDแยกจาก3.3Vในexport/copper check | DIFFERENT | MCUอาจไม่มีไฟหากไม่มีจัมเปอร์ | ตรวจการแก้บอร์ดจริง + continuity |
| MOSFET | MDP10N27 ×6 | IRFS7530 ผู้ใช้ยืนยัน; footprint7pin | DIFFERENT | VDS60V; timing/modelเปลี่ยน | ยืนยันsuffix-7P/ratingsจริง |
| FET count | 2/phase, 6 total symbols | 2/phase, 6 total symbols | SAME schematic | ไม่พิสูจน์ current rating | เทียบ BOM/Gerber |
| Gate driver | EG3112×3 | EG3112D×3 ตาม PDF | PARTIAL | GPIO driver HAL, ไม่ใช้ DRV83xx SPI | ยืนยัน suffix/marking/truth table |
| PWM interface | 6 HIN/LIN, active-high EG3112 | 6 HIN/LIN ตาม net | PARTIAL | TIM1 complementary | bench verify polarity |
| PWM pins | PA8/9/10, PB13/14/15 | pins เดียวกัน ผ่าน 0Ω | SAME | board mapping ไม่ต้องสลับเฟส | continuity R73–R78 |
| EN / FAULT / config | ไม่มี dedicated pins บน EG3112 | ไม่พบ | SAME absence | capability=unavailable | ห้ามรายงาน fault=false ว่า hardware healthy |
| Dead-time | header 660ns | ไม่กำหนดจริง | UNKNOWN | ต้อง encode BDTR ใหม่ | วัด VGS overlap |
| Gate R / Cgs | 10Ω /22nF | 10Ω /22nF | SAME nominal | รวม Cgs ใน gate-charge delay | ตรวจ population/edge |
| Bootstrap | FR107 /10µF /12V | เหมือน nominal | PARTIAL | จำกัด maximum on-time, precharge | วัด VB−VS ต่ำสุด |
| DC-link | 2.2µF×3 + buck input | C18–20 2.2µF×3 + input C60 | PARTIAL | ไม่อนุมาน 100A capability | bulk cap/ripple/layout confirmation |
| Bus TVS | SMCJ85A | ไม่พบ | DIFFERENT | regen ไม่ได้รับการ clamp เทียบ reference | ตรวจของจริง/ออกแบบแรงดัน |
| Snubber | ไม่พบ dedicated RC D-S | ไม่พบ dedicated RC D-S | SAME observed absence | overshoot ต้องวัด | scope VDS/phase |
| Current path/Kelvin | low-side schematic,ไม่มีreferenceGerber | customGerber4ชั้น มีpowercopper/via | DIFFERENT layout; rating UNKNOWN | noise, offset, protection latency | รายละเอียดGERBER_REVIEW_TH + วัดKelvin/thermal |
| Shunt | 0.5mΩ×3 parallel/phase | R47/50/53 1mΩ/phase | DIFFERENT ×6 | scale/current limits ต้องเปลี่ยน | Kelvin resistance + pulse rating |
| Amplifier/gain | INA181A1,20 | INA181A1,20 ผู้ใช้ยืนยัน | SAME part/gain | gain ไม่ได้ทำให้ scale เดิมใช้ได้ | verify 3 channels |
| Current polarity/offset | IN+ GND /IN− source, half-rail | IN+ AGND /IN− CUR, half-rail | SAME schematic | sign convention ต้องสอดคล้อง FOC | known-current test |
| Current ADC | PC0/1/2 CH10/11/12 | same | SAME | synchronized triple ADC | remap ranks explicitly |
| Output current C | 1nF | PDF100nF; ผู้ใช้ถอดแล้ว | DIFFERENT as-built | settling/noise เปลี่ยน | ยืนยัน C39/42/45 และวัด |
| Input current C | 100nF across shunt | C37/40/43 100nF | SAME nominal | ไม่ใช้ 1k REF R คำนวณ current RC | ตรวจ input routing |
| Phase dividers | 56k/2.2k → PA0/1/2 | R91–96 → PA0/1/2 | SAME | phase scale ≠ physical VBUS reading | verify transient range |
| Phase filter | ไม่มี switched filter | ไม่มี switched filter | SAME absence | HW_HAS_PHASE_FILTER=0 | ห้าม define plural VESC flag เป็น 0 ภายใต้ #ifdef |
| VBUS | 560k/21.5k → PC3 | PC3 ไม่มีสายต่อ | DIFFERENT / MISSING | UV/OV/regen blocked | เพิ่ม/ยืนยัน direct divider |
| FET temperature | T1 NC, PA3,10k lower | PA3 ว่าง ไม่มี NTC | DIFFERENT | ไม่มี thermal protection measurement | เพิ่ม sensor + R25/B |
| Motor temperature | connector TEMP→PC4 | PC4 เป็น SW3 | DIFFERENT | ห้าม scan เป็น motor NTC | disable unsupported channel |
| Hall/ABI | CN2 PC6/7/8, TIM3 candidate | PC6/7/8 ว่าง ไม่มี connector | DIFFERENT | sensorless only initial architecture | remove Hall/ABI paths |
| SPI encoder | generic SPI1 macros, ไม่ยืนยัน dedicated encoder | ไม่มี encoder circuit | SAME not fitted/unsupported | ไม่มี AS5047 support target แรก | ไม่เปิด SPI encoder |
| CAN | MCP2551 PB8/PB9, termination NC | U29 PB8/PB9, R84 fixed120Ω | PARTIAL | CAN1 same AF; bus topology ต่าง | ตรวจ total termination |
| UART | USART3 PB10/11; UART4 PC10/11 connectors | MCU pins ว่าง ไม่มี connector | DIFFERENT | UART ต้องใช้ test wires ที่ยืนยันก่อน | choose routed CAN for first debug |
| USB | PA11/12 +22Ω series | PA11/12→U28, ไม่มี series22Ω ใน PDF | PARTIAL | USB stack optional; electrical QA | ไม่ backfeed external5V |
| PPM | PB6 TIM4,1k/10nF | H4 PB6 R87/C81 | SAME nominal | keep optional, idle disable | verify input level |
| Analog throttle/I2C | external ADC and shared UART/I2C connector | ไม่มี connector/circuit | DIFFERENT | remove initial drivers | ไม่ใช้ floating pins เป็น throttle |
| Buttons | reference PC4 temp, PC5 spare ADC | PC4/SW3,PC5/SW4 | DIFFERENT | GPIO digital, debounce | map stop/arm after test |
| LEDs | GPIO PB5/PB7 | LED1 power only | DIFFERENT | no status LED toggling | debug via communication |
| Hardware break | ไม่เห็น comparator/BKIN net | PB12 NC,ไม่มี comparator | SAME missing path | cannot claim fast hardware OC | comparator/BKIN hardware change |
| Battery current | ไม่มี dedicated bus sensor | ไม่มี dedicated bus sensor | SAME absence | estimate for control only | ไม่เรียก estimate ว่า bus OC measurement |

## ขอบเขต source และงานที่ยังไม่ผ่าน

หลังตรวจ schematic/Gerber และสร้าง diff แล้ว จึงสร้าง target `minimal_port/boot_debug` สำหรับphase5–6: MCUboot,GPIOsafe state และSWDdebug mailbox โดยไม่เปิดPWM/ไม่armมอเตอร์. งานที่ยังต้องมีการแก้hardwareหรือผลวัดคือ MCUrail, directVBUS, temperature, fastshutdown และtiming. การหมุนมอเตอร์,FOC,observer,speedcontrol,parameterdetection ยังไม่implemented/bench-validated ในtargetใหม่นี้ ตามลำดับbring-upที่ผู้ใช้กำหนด

ไม่ใช้ผล compile ของ `bldc_lite_24` เดิมเป็นผล compile ของ minimal FOC ใหม่ และไม่ถ่าย fixed nominal VBUS หรือ fabricated temperature จากชุดทดสอบเดิมมาอ้างว่าเป็น sensor จริง
