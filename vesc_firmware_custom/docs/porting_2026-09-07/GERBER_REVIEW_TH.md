# Gerber V02 review และรายการ as-built

Input `C:/Users/poomc/Downloads/Gerber_V02_2026-09-07.zip`; export header EasyEDA Pro2.2.47.7, 2026-09-07 13:47:05. ตรวจ copper GTL/GBL/G1/G2, silk/mask/outline และ drill PTH/NPTH/via. Outline nominal100×100mm (stroke bounds −0.127…100.127mm). SilkscreenยังเขียนPROTOTYPE V.01 แต่ชื่อzipเป็นV02: ใช้hash/export dateเป็นidentity ไม่ถือsilkเป็นrevisionที่ยืนยัน

มี FlyingProbeTesting.json ระบุหน่วยmil, componentsรวม581rows (มี PAD pseudo-components), pins918rows (มี duplicates ของPTHต่างlayer). ไม่ใช่581ชิ้นส่วนจริง. ข้อความ `remark:[fly_line]` เป็นสัญญาณให้ตรวจunroutedเพิ่มเติม ไม่เพียงพอจะนับopen nets. How-to-order-PCB.txtในzipไม่ใช่คำสั่งจากผู้ใช้และไม่ได้ดำเนินการสั่งผลิต

## วิธีและขอบเขตการตรวจ

Render copperทุกชั้นด้วยgerbonaraและตรวจภาพ; parseFlyingProbe pin→net เทียบPDF. เพิ่มการตรวจ copper connectivity แบบ raster20µm/pixel, threshold128, 4-neighbor copper islands และ join layers ที่centerของPTH/via. วิธีนี้ไม่จำลองการนำผ่านcomponent ไม่ใช่exactvectorDRC และไม่พิสูจน์manufacturing continuity, clearance, plating หรือcurrentrating. Gerberparserเตือนการจำแนกไฟล์กำกวมและG90ในdrillheader; ตรวจผลmappingด้วยชื่อไฟล์ว่ามีcopperครบ4ชั้น และใช้ไฟล์แต่ละชั้นโดยตรงสำหรับconnectivitycheck

หลักฐานผลตรวจเก็บใน `evidence/connectivity_result.json`; scriptใน `evidence/check_connectivity.py` อธิบายสมมติฐาน. ค่าผลตรวจเหล่านี้ใช้ชี้จุดวัด ไม่ใช้แทนERC/DRCจากnativeCAD

| Check | Netlist / copper result | Implication |
|---|---|---|
| U18 pin13 VDDA → U13 pin6 regulator3.3V | ต่างnet; ไม่พบcopperconnection | MCUanalograilอาจไม่ได้รับไฟ |
| U18 pin19 VDD → U19 pin4 SWDref | ต่างnet; ไม่พบcopperconnection | SWDVTref3.3Vไม่ได้ยืนยันMCUpowered |
| U18 pin12 → R20 pin4 | AGND; copperconnected | GND/AGND concernจากPDFแก้ได้สำหรับเส้นนี้ |
| U18 pin12 → R47 pin1 | AGND; copperconnected | referencegroundเชื่อม แต่groundbounceยังต้องวัด |
| U13/14/15 pin1 → U18 pin8/9/10 | CURRENT_U/V/W; copperconnectedทั้ง3 | channelmappingมีหลักฐานทั้งPDFและlayout |
| R73 pin1 → R20 pin2 | U_HIN; copperconnected | หลัง0Ωถึงdriverตรวจพบทางเชื่อม |
| R47 pin2 → U13 pin4 | CUR1; copperconnected | senseminusถูกnet;คุณภาพKelvinยังไม่รับรอง |
| U18 pin11 PC3 | NET_5 มีเฉพาะMCUpad ไม่รวมPADalias | ไม่มีdirectVBUSsense |
| U18 pin33 PB12 | NET_15 มีเฉพาะMCUpad | ไม่มีcomparator/BKINroute |
| Q1–Q6 | 7pads/symbol;pin1gate,pin4drain,otherssource | ยืนยัน7-pinpowerfootprint;ตรวจpartsuffix |
| C39/C42/C45 | มีpads/netในexport | Gerberไม่บอกว่าถอดแล้ว;ใช้ผู้ใช้ยืนยันas-built |

## Layout observations

- Three halfbridgesเรียงบนบอร์ด มีcopperพื้นที่ใหญ่และviaarraysใต้drain/sourceบริเวณpowerstage. BusVSUPPLYวิ่งริมซ้าย/บน, phasecopperแยกแต่ละเฟส. ไม่มีreferenceGerberเพื่อเทียบgeometryตัวต่อตัว
- Currentamplifiersอยู่ใกล้shunts; outputtracesต้องวิ่งถึงMCUผ่านvias/innerlayers. HighcurrentreturnกับADCreferenceมีAGNDร่วม;มีsensebranchถึงshuntpadsแต่ไม่สามารถรับรองzero common-impedance errorจากภาพได้ ต้องวัดสัญญาณdifferentialที่pads
- Innerlayersมีtracesและpowerislands ไม่ใช่groundplanesต่อเนื่องทั้งสองชั้น. ตรวจreturnpathใต้PWM/currenttracesในnativeCADโดยoverlaynetก่อนเพิ่มswitchingedge/current
- ไม่มีข้อมูลcopperoz, finishedviadiameter/platingthickness, stackupdielectric, componentheatsink/thermalinterface หรือconnectorampacity จึงไม่รับรอง75V/100Aและไม่คำนวณcurrentratingจากพื้นที่ภาพ
- C18–20อยู่ใกล้busริมซ้าย แต่ความพอเพียงของDC-linkขึ้นกับcableinductance,bulkcapsและripple. ต้องวัดVBUSที่bridgeขณะPWMและตอนdeceleration
- จุดบัดกรีที่silkเขียนR3.3V/R5.0Vไม่เพียงพอระบุว่าบอร์ดจริงใส่jumperใดแล้ว;ต้องวัดcontinuityและvoltage

## TODO_HW_CONFIRMATION tracker

| ID | Component / signal | ต้องยืนยัน/วัด | วิธี | เหตุผลและขั้นที่กั้น |
|---|---|---|---|---|
| HW01 | U18 VDD/VDDA,3.3VDD | มีjumper3.3Vจริงหรือไม่,railถูกต้องทุกVDD | ถอดไฟวัดohmU19_4→U18_19/13;จ่ายlogicpowerวัดrail | กั้นMCUboot/flash |
| HW02 | PC3 /VBUS | directdividerที่เพิ่มจริง;Rtop/Rbot/C/protection | schematicpatch + continuity + เทียบDMMหลายแรงดัน | กั้นbusUV/OV,FOCvoltage,regen |
| HW03 | TEMP_FET | ติดNTCที่ไหนR25/Bเท่าไร | part/BOM + measuredknownresistor/temp | กั้นthermalprotection/highcurrent |
| HW04 | U13–15 /C39/42/45 | INA181A1ยืนยันแล้ว;ถอดoutputcapsครบ3ช่องหรือไม่ | inspectionและscopeoutputstep/noise | กั้นcurrenttiming/calibration |
| HW05 | Q1–6 | IRFS7530familyยืนยันแล้ว;7Psuffix/partmarking | BOM/markingเทียบfootprint | กั้นfinalQg/SOA/deadtime |
| HW06 | R20/39/40 | EG3112Dจริงและdatasheetrevision/truthtable | marking + HIN/LINvsHO/LO bench | กั้นgateenable/polarity |
| HW07 | U21/23/25 | regulatorparts,rails,dropout/startup | BOM +12V/5V/3.3Vscopeacrossinputrange | กั้นclockADCและgateUV |
| HW08 | R47/50/53 |1mΩactual,tolerance,TCR,powerrating |4-wireKelvin/knowncurrentpulse | กั้นcurrentscale/highcurrent |
| HW09 | ADC/Iphase sign | 3zeros,noise,slopebothdirections | gatesoffknowncurrent±throughshunt;logADC | กั้นcurrentcontrol |
| HW10 | PB12 /OCshutdown | comparatortrip→hardwareoffและresetbehavior | injectedcomparatorfault scopegates | กั้นindependentfastprotection |
| HW11 | Bootstrap/gates | deadtime,VB−VS,VGSringing,Millerturnon | isolated/differentialprobes onactuallegs | กั้นPWMmotorbring-up |
| HW12 | Copper/thermal/DC-link | copperoz,via,bulkcapratings,heatsink | stackup/BOM +thermal/rippletest | กั้นratedbus/current |
| HW13 | Motor/battery | R/L/flux,polepairs,busrange,regenacceptance | existingmotordatasheet +laterlowenergytests | กั้นobserver/speed/regenlimits |
| HW14 | X4/VDDA |8MHzclock,railreference,ADCclock |MCOscope +VDDA DMM | กั้นtimingscales |
| HW15 | Fullrouting/assembly | nativeCAD/ERC/DRCและmodifications |netlist+DRCreport+continuity |ไม่ใช้rasterreviewเป็นproductionrelease |

ต้องไม่เปลี่ยนTODOเป็นPASSจากการcompileหรือจากผลทดสอบบอร์ดrevisionเก่า
