<div align="center">

<a href="hardware/README.md"><img src="docs/assets/esc-foc-drive-board-3d.png" alt="ESC FOC Drive V02 PCB" width="74%"></a>

# ESC-FOC-Drive-V02

**STM32F405 three-phase BLDC/FOC engineering platform with Arduino six-step and SAT motor pilot app for Windows**

[![Release](https://img.shields.io/badge/release-v1.0.0--app-2563EB?style=for-the-badge)](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.0.0-app)
[![MCU](https://img.shields.io/badge/MCU-STM32F405RGT6-0B1220?style=for-the-badge&logo=stmicroelectronics&logoColor=38BDF8)](https://www.st.com/en/microcontrollers-microprocessors/stm32f405rg.html)
[![Status](https://img.shields.io/badge/status-engineering_prototype-DC2626?style=for-the-badge)](#safety-warning)
[![English selected](https://img.shields.io/badge/English-selected-2563EB?style=for-the-badge)](README.md)
[![Switch to Thai](https://img.shields.io/badge/%E0%B9%84%E0%B8%97%E0%B8%A2-switch-475569?style=for-the-badge)](README_TH.md)

[Open the web app](https://poommyroboticcamera-netizen.github.io/ESC-FOC-Drive-V02/) · [Download the app](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.0.0-app) · [Select Firmware](#available-firmware) · [How to use the app](#start-using-sat-motor-pilot) · [Hardware](hardware/README.md) · [old model](#the-old-version-is-still-kept)

</div>

The SAT motor pilot web app starts in English. Use the language button beside the connection controls to switch to Thai.

---

## Recommended version: v1.0.0-app

This version adds **SAT motor pilot** for viewing status. Initial tuning, six-step control and phase telemetry recording via USB CDC with firmware <code>v33-proto1</code> that are specifically used together

Download separated by model from [GitHub Releases](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases) Where **One Release contains files of that release only**:

| Release / Version | download file | used for |
|:--|:--|:--|
| [SAT motor pilot v1.0.0](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.0.0-app) | <code>SAT-motor-pilot-1.0.0-win-x64.exe</code> | Portable Windows x64 apps |
| [Arduino six-step app v33-proto1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-app-v33-proto1) | <code>stm32f405-sixstep-app-v33-proto1.zip</code> | Firmware used with the app only |
| [Arduino six-step current v1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-current-v1) | <code>stm32f405-sixstep-current-v1.zip</code> | Six-step via Serial Monitor, current measurement model; **Not used with apps** |
| [Arduino six-step no-current v1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-no-current-v1) | <code>stm32f405-sixstep-no-current-v1.zip</code> | Six-step via Serial Monitor without measuring current; **Not used with apps** |

The SHA-256 value of the file is in each release's description. Download only the required release and do not flash multiple versions of firmware together.

> [!IMPORTANT]
> SAT motor pilot supported only <code>fw=v33-proto1</code>, <code>protocol=1</code> and <code>current_protection=OFF</code> The app rejects different firmware versions to avoid sending commands against the protocol.

## Available firmware

| Profile | Source | Current sense | How to control | status |
|:--|:--|:--|:--|:--|
| App v33 | [stm32f405_sixstep_app_v33](arduino_sixstep_releases/stm32f405_sixstep_app_v33) | Diagnostic only, Unit A not yet confirmed. | SAT motor pilot | Recommended version for the app |
| Current | [stm32f405_sixstep_current](arduino_sixstep_releases/stm32f405_sixstep_current) | INA181 U/V/W + software trip | Serial Monitor | Bench bring-up |
| No current | [stm32f405_sixstep_no_current](arduino_sixstep_releases/stm32f405_sixstep_no_current) | Close Do not read PC0/PC1/PC2 | Serial Monitor | Logic/gate test |
| Custom VESC | [vesc_firmware_custom](vesc_firmware_custom) | Board-specific current path | VESC Tool / terminal | Separate development work |

Read details on selecting and flashing at [Arduino six-step release guide](arduino_sixstep_releases/README_TH.md)

- **App v33** includes heartbeat 1500 ms, watchdog, telemetry, phase stream and scope capture.
- **Current** has zero calibration and software trip, but not hardware short-circuit protection.
- **No current** No current feedback or software trip. Current must be limited with an external supply.
- **Custom VESC** is a different codebase. Do not flash and switch without checking the target and manual.

Library [SolidGeek/VescUart](https://github.com/SolidGeek/VescUart) Not used with SAT motor pilot because VescUart uses VESC binary packet protocol. This app uses ASCII protocol of <code>v33-proto1</code> Via USB CDC

## Start using SAT motor pilot

open [SAT motor pilot on the web](https://poommyroboticcamera-netizen.github.io/ESC-FOC-Drive-V02/) You can do this from Edge or Chrome on the computer where the board is plugged in via USB. The website uses Web Serial over HTTPS and requests permission to select the port from the browser. Serial data is between the browser and the board on the user's machine. It is not a server controlled motor. Mobile websites or browsers that do not support Web Serial cannot be used to connect to the board.

Thai font used by the website `Noto Sans Thai` From Google Fonts, which is an open source font with a smooth shape similar to Aktiv Grotesk Thai. If the font cannot be loaded, a Thai font will be used as an alternative.

### 1. Flash the firmware that matches the app.

1. download <code>stm32f405-sixstep-app-v33-proto1.zip</code> Then extract the file
2. open <code>stm32f405_sixstep_app_v33.ino</code> With Arduino IDE 2
3. Install the package **STM32 MCU based boards** of STMicroelectronics.
4. Select Board <code>Generic STM32F4 series</code>, part <code>Generic F405RGTx</code>
5. Select USB <code>CDC (generic Serial supersede U(S)ART)</code> and Low/Full Speed
6. choose <code>STM32CubeProgrammer (SWD)</code> When using ST-LINK
7. Press **Verify** before uploading without connecting the motor and not paying the DC bus.

### 2. Open the app

1. download <code>SAT-motor-pilot-1.0.0-win-x64.exe</code>
2. Open the app file as portable, no need to install Python.
3. Connect STM32F405 with USB data cable.
4. Press refresh, select the COM of the board, then press **Connect board**
5. The app detects HELLO, starts sending HEARTBEAT, reads STATUS and automatically turns on phase stream.

If you cannot open COM, close the Arduino Serial Monitor, Arduino IDE, browser tab, or SAT motor pilot in another window that is using the same port and check the COM number in Windows Device Manager.

### 3. Test the motor

1. Use a low voltage source and low current limit.
2. Prepare an accessible DC bus cutoff switch.
3. Connect the app and check that the status is <code>DISARMED</code>
4. Press ARM and set duty/startup profile to low.
5. Press RUN and monitor state, fault, duty, phase ADC and BEMF lock.
6. Press STOP immediately when the motor vibrates, freezes, overheats, or the current increases abnormally.

Phase U/V/W values ​​are raw ADC counts, not calibrated voltages. Unverified current, RPM, bus voltage, and temperature values ​​are displayed as N/A. The app does not generate simulated values.

## App functions

- ARM / RUN / STOP and signed duty
- Direction, initial startup period, acceleration ramp และ duty response
- State, fault, commutation timing, BEMF lock และ heartbeat
- Phase U/V/W live trend max approximately 10 samples/s
- Scope snapshot up to 512 PWM-ON samples and live short captures
- Export CSV from actual received data
- Serial console for diagnostic
- English/Thai UI

The source is in [bldc_tuner_windows](bldc_tuner_windows) If you want to run it through your browser, open it. <code>Start SAT motor pilot.bat</code> with Python 3 or build Electron included <code>npm install</code> and <code>npm run dist:win</code> in <code>bldc_tuner_windows/electron-shell</code>

## Hardware overview

| list | details |
|:--|:--|
| MCU | STM32F405RGT6, ARM Cortex-M4F |
| Power stage | Three-phase, six MOSFET bridge |
| Gate driver | EG3112D, 3 channels |
| Current sense | INA181A1 gain 20 V/V, shunt 1 mΩ x3 |
| Interfaces | USB CDC, CAN, PPM, SWD |
| Main bring-up target | 24 V class, low-power controlled bench |

### Hardware pin map

| Function | STM32F405 pin |
|:--|:--|
| High-side U / V / W | PA8 / PA9 / PA10 |
| Low-side U / V / W | PB13 / PB14 / PB15 |
| Current U / V / W | PC0 / PC1 / PC2 |
| Phase voltage U / V / W | PA0 / PA1 / PA2 |
| Stop SW3 / SW4 | PC4 / PC5, active LOW |
| USB FS DM / DP | PA11 / PA12 |
| CAN1 RX / TX | PB8 / PB9 |
| SWDIO / SWCLK | PA13 / PA14 |

Production files and circuit drawings:

- [Hardware guide](hardware/README.md)
- [Latest Schematic](hardware/SCH_drive_2026-09-22.pdf)
- [Latest PCB PDF](hardware/PCB_V02_2026-09-22.pdf)
- [BOM](hardware/BOM_BLDCv02.xlsx)
- [Pick-and-Place](hardware/PickAndPlace_BLDCV02.xlsx)
- [Gerber](hardware/Gerber_BLDCV02.zip)

## The old version is still kept.

The new version does not overwrite or delete the old source:

| original folder | content |
|:--|:--|
| [arduino_stm32f405_bldc_test](arduino_stm32f405_bldc_test) | STM32F405 open-loop bring-up original model |
| [arduino_eg2132_bldc_test](arduino_eg2132_bldc_test) | Arduino Uno/Nano gate-input test original version |
| [vesc_firmware_custom](vesc_firmware_custom) | Custom VESC source, reports and original development work |

If you must rollback, checkout the desired commit/tag or use the source in the original folder. Do not assume that the protocols and commands of each version are compatible.

## Repository structure

    arduino_sixstep_releases/         Current / no-current / app-v33 profiles
    bldc_tuner_windows/               SAT motor pilot source
    arduino_stm32f405_bldc_test/      Legacy STM32 bring-up (kept)
    arduino_eg2132_bldc_test/         Legacy external Arduino test (kept)
    vesc_firmware_custom/             Custom VESC development tree
    hardware/                         Schematic, PCB, BOM, PnP, Gerber
    docs/assets/                      README images

Generated executables, Electron dependencies, build output and temporary files are not committed; Download binary from GitHub Releases

## Verification status

| list | result |
|:--|:--|
| SAT motor pilot JavaScript syntax | pass |
| Python localhost launcher | Passed, HTTP 200 |
| Electron Windows x64 portable build | pass |
| App ↔ firmware handshake fields | Checked for consistency. |
| Release ZIP structure / SHA-256 | pass |
| Arduino v1 profiles compile in this release. | You must press Verify in the Arduino IDE because the build machine does not have the Arduino CLI. |
| Tested with real boards and motors. | Still need to process |

Compiling and launching the app does not verify ADC timing, dead time, gate waveform, current accuracy, or fault response on the actual power sector.

## Safety warning

> [!CAUTION]
> This project is an engineering prototype, not an ESC production or safety-rated system. The power section can generate high current, heat, and reverse voltage. and unexpected rotation

- Uses fuses, current limiting supplies and DC bus cut-off switches.
- Check gate input and gate waveform without connecting the motor first.
- Use a differential/isolated probe when measuring high-side. Do not connect the oscilloscope ground clip to a phase node or HO.
- Software trip, watchdog, heartbeat, and STOP button do not replace hardware over-current shutdown.
- Do not increase duty to cause the motor to stall or vibrate.
- Not yet approved for use in automotive, medical, human-carrying or safety-critical systems.

There is no repository-wide license for project-specific files. Making them public does not automatically grant license or distribution. VESC and third-party components are licensed by the source.
