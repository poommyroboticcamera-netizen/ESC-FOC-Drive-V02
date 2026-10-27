# Arduino six-step app firmware v34-proto1

**Known hardware erratum (2026-10-27):** On the assembled ESC V02 board, C39/C42/C45 are 100 nF directly from each INA181A1 OUT to AGND. [TI specifies](https://www.ti.com/lit/ds/symlink/ina181.pdf) 1 nF maximum capacitive load for no sustained oscillation. Stopped zero calibration measured 925–1057 ADC counts of trimmed spread and correctly refused to calibrate. Do not use the current display or weaken its zero check. See the [rework and retest instructions](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/hardware/CURRENT_SENSE_ERRATUM_TH.md). The published ELF binary is unchanged; board rework and retesting are required.

For **STM32F405RGT6** ESC V02 with INA181A1 U13/U14/U15 and 1 mΩ shunts R47/R50/R53 populated. This release is separate from and does not replace v33-proto1 or the current/no-current Serial Monitor profiles.

Assets:

- `stm32f405-sixstep-app-v34-current-nominal.elf` — flash with ST-LINK/SWD in STM32CubeProgrammer.
- `stm32f405-sixstep-app-v34-current-nominal.zip` — exact Arduino sketch and README.

SHA-256: ELF `14055e3e2f4d24588a5d51c5e82130e58d91d5974905d8ccdfa9a2f5113f2a5e`; ZIP `53132f0c50069f11298bc4f85d356b2db40f260ed7733e359ab259ec02090ed0`.

Changes: app-controlled `CSCAN ON/OFF`, stopped-motor `CZERO`, fresh qualified LOW-side current telemetry in ALIGN/STARTUP/CLOSED_LOOP, and explicit unavailable state. Use with [SAT motor pilot v1.1.0](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/v1.1.0-app) or the updated [web app](https://poommyroboticcamera-netizen.github.io/ESC-FOC-Drive-V02/). v33 remains supported by the new app but cannot display this current value.

The displayed amperes are calculated from **nominal** INA181A1 gain 20, shunt 1 mΩ, and ADC VDDA 3.3 V after RAM zero calibration. Board-specific polarity, scale, timing, and phase mapping require measurement against a known reference. `current_protection=OFF`: this firmware has no ampere trip. Supply current is not the same as a single LOW-side shunt sample. Use an external current limit and physical cutoff.

Build: Arduino CLI 1.2.0, STM32 MCU based boards 3.0.0, Generic F405RGTx, USB CDC generic Serial, Low/Full Speed, `-Os`, SWD. Compile succeeded; bench flash, live ADC behavior, and motor operation remain unverified for this build. See the [v34 README](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/stm32f405_sixstep_app_v34/README.md) and [ELF flash guide](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/ELF_FLASH_GUIDE_TH.md).
