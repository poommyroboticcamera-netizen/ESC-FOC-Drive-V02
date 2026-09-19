# CUSTOM V02 BOOT / DEBUG — พร้อมไฟล์แฟลช

**รุ่นนี้ใช้ตรวจ MCU และ safe GPIO เท่านั้น ไม่ใช่ firmware สำหรับหมุนมอเตอร์ / FOC.** ไม่มี PWM waveform, ADC acquisition, observer, speed loop หรือ VESC Tool protocol. ทุกคำสั่ง ARM ถูกปฏิเสธ; บังคับ PA8/9/10 และ PB13/14/15 ต่ำ, TIM1 MOE/AOE/CCER ปิด. เหตุผลคือ custom board ยังไม่มี direct VBUS, FET temperature และ comparator/BKIN path ตาม protection ที่ร้องขอ

ผู้ใช้ยืนยัน INA181A1, IRFS7530, ถอด output100nF แล้ว และมีจัมเปอร์3.3V→3.3VDD บนบอร์ดจริง. Firmwareไม่สามารถตรวจว่าจัมเปอร์หรือการถอดCถูกต้องจากข้อมูลซอฟต์แวร์ ต้องวัดจริงก่อนจ่ายไฟ

## ไฟล์

- `output/custom_v02_boot_debug.hex` — แนะนำสำหรับ STM32CubeProgrammer;มีaddressในไฟล์
- `output/custom_v02_boot_debug.bin` — raw binary,โหลดที่ **0x08000000**
- `output/custom_v02_boot_debug.elf` — symbols/debugger
- `output/custom_v02_boot_debug.map` — link map
- `output/sha256.json`, `artifact_verification.json`, `host_test_result.txt` — ผลตรวจ

Target STM32F405RGT6, flash1MiB, SRAM128KiBปกติ;ไม่ใช้CCM. CPUใช้HSI nominal16MHzในขั้นนี้เพื่อไม่ผูกbootกับHSEที่ยังไม่วัด. ไม่มีheap,libc,RTOS,USBstackหรือdependencyจากVESCทั้งrepository. Flash **1,104 bytes**, data0, bss64bytes;ตัวเลขRAMไม่รวมstackที่เผื่อ8KiB. ARM GNU14.2.1, freestandingC, `-Wall -Wextra -Werror`; finalcompile/linkไม่มีwarning

## วิธีแฟลช

1. ถอดมอเตอร์และปิด/แยกDCbusกับgate-driver supply. จ่ายเฉพาะlogicpowerด้วยวิธีที่ตรวจแล้วว่าไม่backfeedregulator. ก่อนCPUเริ่มทำงาน GPIOยังเป็นresetstate; binaryนี้ไม่สามารถควบคุมช่วงก่อนreset handlerได้
2. วัด3.3Vที่U18 VDD/VDDAและตรวจจัมเปอร์จริง. U19 pin4คือ3.3V reference, pin3=SWDIO, pin2=SWCLK, pin1=AGND ตามPDF/Gerber. อย่าสลับpinจากทิศมองconnector
3. ใช้ST-LINKต่อSWD. U19ไม่มีNRST;โหมดUnder Resetต้องต่อNRSTเพิ่มที่resetnet/SW1. ถ้าใช้4สายเดิมเลือกNORMAL
4. สำรองfirmware/configเก่าที่ต้องใช้ก่อนเขียน. Binaryนี้เป็นapplicationเริ่ม0x08000000และจะแทนapplicationเดิม ไม่ใช่VESCToolupdatepackage;ไม่ทำmass erase/RDP/option-bytechange
5. จากโฟลเดอร์นี้เรียก `./flash.ps1` ในPowerShell หรือใช้GUIเลือกHEXแล้วDownload/Verify/Reset. Scriptจะตรวจhashก่อนเขียนและสั่งverifyหลังเขียน

```powershell
# รันจากโฟลเดอร์ boot_debug ที่แตกจาก ZIP หรือใน repository
.\flash.ps1
# ใช้ได้เมื่อต่อ NRST จริงเท่านั้น
.\flash.ps1 -UnderReset
```

คำสั่งตรง (แก้pathไฟล์ให้ตรงตำแหน่งที่แตกZIP):

```powershell
$programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
& $programmer -c port=SWD freq=1000 mode=NORMAL -w '.\output\custom_v02_boot_debug.hex' -v -rst
```

ถ้าเลือกBINแทนHEXต้องใส่address0x08000000หลังชื่อไฟล์. CLIflagsตรวจจากinstalled`--help`และ[ST command documentation](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html). **งานรอบนี้สร้างไฟล์และตรวจofflineเท่านั้น ยังไม่ได้เชื่อมprobe/แฟลชหรือทดสอบบอร์ด**

## ตรวจว่า MCU ทำงาน

เปิดELFในdebuggerและwatch `g_debug` หรืออ่านRAM64bytesที่0x20000000. อ่านขณะrun/hotplugเพื่อเห็นheartbeatเพิ่ม:

```powershell
& $programmer -c port=SWD freq=1000 mode=HOTPLUG -r32 0x20000000 0x40
```

| Offset | Field | ค่าที่คาด |
|---|---|---|
| +0x00 | magic | 0x42444731 |
| +0x04 | abi | 1 |
| +0x08 | uptime_ms | เพิ่มตามHSI nominal1ms;ไม่ใช่precisionclock |
| +0x0C | state | 1=DEBUG_READY,2=DEBUG_FAULT;ไม่ใช่MOTOR_READY |
| +0x10 | reset_cause | RCC_CSRsnapshot |
| +0x14 | blockers | 0x0F:VBUS,temp,HW_OC,unverifiedbringup |
| +0x18 | gate_levels | 0;bit0..2=PA8/9/10,bit3..5=PB13/14/15 |
| +0x1C | buttons | bit0=SW3pressed,bit1=SW4pressed |
| +0x20 | heartbeat | เพิ่มขณะmainทำงาน |
| +0x24 | fault_code | 0normal,1exception,2clockfailure,3gateinputreadbackhigh |
| +0x28 | request_seq | debuggerเขียนเป็นตัวสุดท้ายของrequest |
| +0x2C | command | 1PING,2STOP,3ARM,4CLEAR_FAULT |
| +0x30 | argument | ต้อง0 |
| +0x34 | response_seq | เท่ากับrequest_seqเมื่อประมวลผลเสร็จ |
| +0x38 | response | 1OK,2BLOCKED,3BAD_COMMAND |
| +0x3C | arm_rejections | เพิ่มเมื่อปฏิเสธARM |

ส่งdebugrequestโดยเขียนcommandและargumentก่อน แล้วเพิ่มrequest_seqเป็นขั้นสุดท้าย รอresponse_seqตรงกันก่อนส่งใหม่. PING/STOPตอบOK; ARM/CLEAR_FAULTตอบBLOCKEDเสมอ. GPIOreadbackเป็นMCUinputlevel ไม่ใช่การวัดVGS/กระแสมอเตอร์;ต้องใช้scopeที่driver/gatesแยกต่างหาก

## Build / tests

```powershell
.\build.ps1
.\tests\run_tests.ps1 -HostGcc 'path\to\native\gcc.exe'
python .\tests\verify_artifacts.py
```

`build.ps1`ค้นหาARMtoolchainบนPATHหรือSTM32Arduinoinstallation;กำหนด`-ToolchainBin`ได้. `run_tests.ps1` defaultใช้w64devkitในworkspaceเดิมจึงต้องระบุHostGccเมื่อย้ายเครื่อง. Pythonartifactcheckใช้standardlibraryเท่านั้น

ผลทดสอบ:512command/argumentcases + gateenablefailclosed + shutdownจากTIM1active + ไม่remuxSWD. ตรวจELFARM,stack/resetvector,earlygatecallก่อนmain,และHEXchecksum/contentตรงBIN. MMIOtestsไม่จำลองswitchingtransient,resetpinbehaviorหรือelectricalsafetyของบอร์ด

ยังไม่มีwatchdog/protectionสำหรับมอเตอร์ในtargetนี้;faultpathsเป็นMCU/GPIOdiagnosticเท่านั้น. Sourceใหม่ไม่ได้copyVESCalgorithm;เอกสารportingระบุdependency/reuseplanสำหรับขั้นถัดไป
