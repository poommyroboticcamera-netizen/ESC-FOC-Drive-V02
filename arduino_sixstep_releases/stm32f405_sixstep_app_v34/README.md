# STM32F405 six-step app v34-proto1: current monitor

This profile pairs with SAT motor pilot v1.1.0 (web or Windows). It preserves the v33 motor control, heartbeat, phase telemetry, and scope features. It adds an **opt-in current readout**, not current limiting or a calibrated three-phase measurement. The v33 source and release remain available separately.

> **Assembled ESC V02 hardware erratum:** C39/C42/C45 are 100 nF directly on the INA181 outputs; TI specifies a 1 nF maximum capacitive load for no sustained oscillation. A board with all three capacitors installed produced 925–1057 trimmed ADC counts of zero-current spread and `CZERO` correctly rejected it. Read the [current-sense hardware erratum](../../hardware/CURRENT_SENSE_ERRATUM_TH.md) and rework/test the board before relying on current readings. Do not relax the zero-noise limit in firmware.

## Hardware and meaning of the reading

The ESC V02 schematic/BOM lists three INA181A1 amplifiers (U13/U14/U15, nominal gain 20 V/V), three 1 mΩ low-side shunts (R47/R50/R53), and CURRENT_U/V/W at STM32 PC0/PC1/PC2. The installed board was reported to have these populated. The firmware assumes 3.3 V ADC reference, the BOM values, and a U/V/W pin mapping. **Neither actual VDDA nor current polarity, phase mapping, gain, shunt value, and acquisition timing has been verified on this assembled board.** Compare with a known signed current before relying on the ampere estimate.

While the motor drives, the reported `current_estimate_mA` is **one PWM-ON sample of the presently selected LOW-side shunt**, with `current_phase=U|V|W`, not simultaneous U/V/W currents, input/battery current, RMS, peak, Id, or Iq. `current_sample=0` with `current_estimate_mA=na` means no qualified fresh sample. The app hides stale samples and labels the value `~` for nominal estimate. No current threshold trips the motor; `current_protection=OFF`. Acquisition failure when scanning is enabled can stop the motor with `fault=current_monitor`; this is not an ampere limit.

## Build and flash

Target **STM32F405RGT6** using Arduino IDE 2 / STM32 MCU based boards 3.0.0: `Generic STM32F4 series`, `Generic F405RGTx`, `CDC (generic Serial supersede U(S)ART)`, Low/Full Speed USB, `-Os`, ST-LINK/SWD. Open `stm32f405_sixstep_app_v34.ino`, or flash `stm32f405-sixstep-app-v34-current-nominal.elf` from the separate [v34 firmware release](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-app-v34-current-nominal) following [the ELF guide](../ELF_FLASH_GUIDE_TH.md). Flash with motor and DC bus disconnected. After reset, `HELLO` must report `fw=v34-proto1`, `protocol=1`, `current_protection=OFF` and `caps` containing `current_monitor`.

## Use with the app

1. Open the [web app](https://poommyroboticcamera-netizen.github.io/ESC-FOC-Drive-V02/) in Edge/Chrome on the computer connected to the board, or the v1.1.0 Windows app, and connect the STM32 USB CDC COM port.
2. Keep the drive stopped and **ensure zero current through all three shunts**. Press **Enable current** (`CSCAN ON`). Wait for monitor status to be ready, then press **Zero calibrate** (`CZERO`). `current_zero=READY` appears after 64 scans, normally about 0.64 s. Repeat after every board reset. The zero is RAM only.
3. First confirm the zero/raw readings in the Diagnostics console with `CURRENT` and `CZEROSTATS`; they should be stable and away from ADC rails. Compare a shunt channel against a known signed DC reference using the existing stopped-only `CTEST` / `CREF` diagnostic procedure. `CREF` reports the comparison and does not adjust firmware scale.
4. Use a low-voltage, externally current-limited supply and physical DC-bus cutoff. During ALIGN, STARTUP, or CLOSED_LOOP the dashboard shows the latest qualified LOW-side sample. If the value remains blank, read `CURRENT` and `CDIAG` in Diagnostics after stopping; a pulse may be too short for an eligible sample.
5. Press **Disable current** (`CSCAN OFF`) while stopped to return to the v33 acquisition path. It does not erase the RAM zero; reset does.

Current scanning is OFF at boot. The app sends heartbeat and polls STATUS; the monitor does not bypass the v33 safety lease. Do not use this estimate as the sole protection against overcurrent. Verify gate timing, ADC window, polarity, and acquisition behavior on the actual board before higher-power tests.

## Serial protocol

`CSCAN ON|OFF` and `CZERO` now answer `OK CSCAN ...` / `OK CZERO ...` or `ERR ...`, so the app can confirm a command. `STATUS` adds `current_monitor_ok`, `current_zero`, `current_sample`, `current_phase`, `current_raw_counts`, `current_zero_counts`, `current_estimate_mA`, and `current_age_us`; all other v33 control fields remain. `current_data=NOMINAL_UNVERIFIED` makes the measurement status explicit. `current_phase` is `-` and `current_estimate_mA=na` when no qualified sample is available.
