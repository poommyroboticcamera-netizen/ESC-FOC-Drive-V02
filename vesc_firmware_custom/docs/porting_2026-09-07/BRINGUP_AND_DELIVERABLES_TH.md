# Bring-up, debug และสถานะส่งมอบ

## ข้อเท็จจริง as-built ล่าสุด

ผู้ใช้ยืนยัน currentampINA181A1, MOSFETIRFS7530, ถอด100nFที่outputcurrentแล้ว และมีjumper3.3V→3.3VDD. Gerberยังเป็นสภาพก่อนการแก้เหล่านี้ในบางจุด;การยืนยันผู้ใช้ไม่ใช่ผลscope/measurement. ให้บันทึกmodificationlistคู่กับrevisionและhash

## ลำดับ bring-up และเกณฑ์ผ่าน

| Step | การทดสอบ | เกณฑ์ผ่าน / จุดหยุด |
|---|---|---|
| 1 MCU+SWD | แยกbus/gatesupply,ตรวจrails/jumper,connectSWD | IDตรงSTM32F405,ทุกVDD/VDDAถูกต้อง |
| 2 Supplies | วัด3.3V/5V/12Vและstartup/dropout | stable,ไม่มีovershootเกินratings;ไม่อนุมานจากpowerLED |
| 3 Boot/debug | แฟลชboot_debug,อ่านmailbox/ปุ่ม | magic/ABIถูก,heartbeatเพิ่ม,gatesreadback0,ARMblocked |
| 4 PWM GPIO | หลังbootผ่าน สร้างPWMtesttargetโดยdriverยังแยกไฟ | PA8/PB13ฯลฯfrequency/polarity/complementaryตรงspec |
| 5 Dead-time | ตรวจpairsทั้ง3ด้วยscope | actualnon-overlap,ไม่ใช้CCR=0แทนalloff |
| 6 Gate outputs | ทดสอบdriverlogicโดยbusยังไม่จ่ายenergyให้bridge | HO−VS/LO−sourceถูก,VGSไม่เกินlimit,bootstraprefreshผ่าน |
| 7 Lowbus | ใช้DCsupplyแรงดันต่ำที่ยังรักษา12Vdriverได้,currentlimit/fuse | ไม่มีshootthrough/ผิดปกติ;ต้องอยู่ในพิกัดอุปกรณ์ |
| 8 Offset calibration | gatesoffเก็บหลายsamplesเช่น1024,mean/stddevทั้ง3 | offset/noiseอยู่ในกรอบจากผลวัด,ไม่มีclip/stuck |
| 9 Manual current | injectedknowncurrentทั้งสองทิศทาง | slope20mV/A,phasepolarity/channelถูกต้อง |
| 10 Open-loop | rotorsecured,lowcurrentlimitedalignment/ramp | currentfeedbackถูก,windowsvalid,ไม่มีovercurrent |
| 11 CurrentFOC | Id/Iqstepเล็กๆ | stablePI/antiwindup/vectorlimit,มีVBUSจริง |
| 12 Observer | speedต่ำไปสูง,ตรวจflux/PLLconfidence | transitionไม่มีanglejump,loss-of-lockตอบสนองได้ |
| 13 Speedloop | lowtorque/speedcommands | limitIq/phase/battery/regenแยกกัน,timeoutหยุด |
| 14 Fault injection | ADCstale/rail,tempopen/short,UV/OV,OC,observerstall,watchdog | latchedfault,gatesoff,noautoresume;ทดสอบโดยไม่ทำลายpowerstage |
| 15 Raisevoltage/current | เพิ่มทีละขั้นหลังทุกก่อนหน้าผ่าน | waveform/thermal/ripplemarginผ่าน;IRFS7530VDS60Vไม่อนุญาต75Vbus |

R/L/fluxdetectและHalltabledetectเพิ่มหลังbasicFOCผ่าน;Hallhardwareยังไม่มีจึงไม่เปิดHalltabletest. OptimizationทำหลังมีDWT/ISRdeadlineและtemperaturemeasurementsจริง

## Oscilloscope / debug checklist

| Signal | จุดวัดตามdesignator/net | สิ่งที่ต้องเห็น |
|---|---|---|
| UH/UL,VH/VL,WH/WL | MCU-sideR73–78 และH2ที่driver-side | frequency,logic,resetbehavior,0Rcontinuity |
| GateU | Q1gate-source,Q2gate-source | VGSamplitude,turnoff,Millerpeak,deadtime |
| GateV/W | Q3/4,Q5/6gate-source | verifyทุกlegไม่ถือเหมือนU |
| Bootstrap | R20/39/40 pin8−pin6 | droop/refresh/undervoltageระหว่างhighduty |
| Phasevoltage | PHASE_U/V/W และdividerR91–96 | overshoot,ringing,samplingtime |
| Currentamplifier | U13/14/15 pin1, removedC39/42/45pads | zero,noise,settlingafteredge |
| CurrentADC | U18 PC0/1/2 | analogsignalตรงchannel,ไม่clip |
| Shunt | R47/50/53ทั้งสองpads differential | knowncurrent/offset/Kelvinerror |
| VBUS | U12VSUPPLY และfutureVBUSdivider | actualbusduringregen/disable |
| Gatefault/enable | ไม่มีpinsในrevisionนี้ | ระบุN/A;หากเพิ่มให้scopeพร้อมBKIN |
| MCUrails | U18 VDD/VDDA, U19_4, C46–50 | jumpereffective,drop/ripple |

ใช้isolated/differentialprobeที่เหมาะกับcommon-modeสำหรับhigh-sideVGS/phase/shunt. ห้ามต่อearthgroundclipของscopeเข้าขาphaseหรือhigh-side source. DCcurrentlimitเพียงอย่างเดียวไม่จำกัดพลังงานที่เก็บในDC-linkcapacitors

ก่อนenergize: verifyfrequency,polarity,deadtime,ADCtriggerpoint,ADCmapping,currentpolarity/gain,VBUSscale,tempcurve,driverfaultcapability,emergencyoffและpower-on/offsequence. Externalpulldown/drivergatingต้องดูแลช่วงMCUreset/debughaltเพราะsoftwareไม่ทำงานช่วงนั้น

## สถานะ 20 deliverables

| # | Deliverable | สถานะ / ที่เก็บ |
|---|---|---|
|1|MKSESCreferenceanalysis|HARDWARE_PORTING_REPORT_TH |
|2|Customschematic/Gerberanalysis|HARDWARE_PORTING_REPORT_TH +GERBER_REVIEW_TH |
|3|Hardwarediff|ตารางFUNCTION/reference/custom/impact/action |
|4–6|Pin/peripheral/ADCmap|MAPPING_AND_CALCULATIONS_TH |
|7|Currentsensecalculations|gain20,1mΩ,signequation,saturation/conditionalrange |
|8|VBUScalculations|reference/phaseคำนวณแล้ว;customdirectVBUSไม่มีhardware |
|9|PWMconfiguration|timingdesignในreport;waveformfirmware/benchยังไม่ผ่าน |
|10|Gatedriverconfiguration|safeGPIOHALimplemented;enableblocked/faultunavailable |
|11|VESCdependencyanalysis|FIRMWARE_ARCHITECTURE_TH |
|12|Minimalarchitecture|designspec + independentboot_debugtarget |
|13|Sourcefiles|`minimal_port/boot_debug`;boot/debugเท่านั้น,FOCยังไม่implemented |
|14|Buildsystem|PowerShell+ARMGNUexplicitfilelist |
|15|Successfulcompile|boot_debugPASS1104Bflash/64Bbss;ไม่ใช่FOCcompileclaim |
|16|Flashprocedure|boot_debugREADME_TH +flash.ps1;ยังไม่แฟลชจริง |
|17|Bring-upprocedure|ตาราง15stepsด้านบน |
|18|Debugchecklist|scope+SWDmailbox |
|19|Safetychecklist|power-off/debuggatechecks+hardwarelimitations |
|20|TODO_HW_CONFIRMATION|GERBER_REVIEW_TH tracker;HW01jumperผู้ใช้ยืนยันแล้วแต่voltageยังไม่วัด |

## ขั้นถัดไปที่ยังต้องใช้หลักฐานจากบอร์ด

ตรวจboot_debugกับSWDและscopegatelow → เติม/ยืนยันdirectVBUSdividerและFETsensor/fastOCpath → ยืนยันpartsuffix/currentCpopulation/rails → PWM/ADCtimingbring-up. ไม่มีการอ้างว่าทดสอบmotoropenloop,currentFOC,observerหรือspeedcontrolสำเร็จแล้วจากcompileเพียงอย่างเดียว
