# SAT motor pilot v1.1.0 — Windows app

**Known ESC V02 hardware erratum (2026-10-27):** Current-sense output capacitors C39/C42/C45 are 100 nF directly on INA181A1 outputs, above the [TI 1 nF capacitive-load specification](https://www.ti.com/lit/ds/symlink/ina181.pdf). The assembled board's `CZERO` test refused to calibrate due to large noise. The app and firmware should keep rejecting that data. Follow the [board rework and retest instructions](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/hardware/CURRENT_SENSE_ERRATUM_TH.md) before using current readout. The published app executable is unchanged.

This Windows x64 portable app adds a LOW-side current card and **Enable current / Zero calibrate** controls for the separate [STM32F405 app firmware v34-proto1](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/releases/tag/arduino-sixstep-app-v34-current-nominal). The same source also runs as a [web app](https://poommyroboticcamera-netizen.github.io/ESC-FOC-Drive-V02/) in Edge or Chrome with Web Serial. v33-proto1 remains supported for existing motor telemetry and controls, without the new current card value.

Asset: `SAT-motor-pilot-1.1.0-win-x64.exe` (portable; no Python required).

SHA-256: `eab178cbfb31ad586a26af9ccc855278abfc1fd9b5af4bb750a3bed94c6f270b`.

The ampere readout is a **nominal estimate of one PWM-ON LOW-side shunt sample** after stopped-motor zero calibration. It is not RMS, Id/Iq, DC-bus current, or a protection limit; the firmware reports `current_protection=OFF`. Calibrate against a known reference before relying on the magnitude or sign. Keep a current-limited supply and physical cutoff for bench testing.

Usage: follow the [hardware rework instructions](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/hardware/CURRENT_SENSE_ERRATUM_TH.md) first; flash v34; open the app and connect its USB CDC COM port; while stopped and with zero phase current press **Enable current**, then **Zero calibrate**; wait for READY; start the motor at low power only after verifying stable sensor outputs. The app hides stale or invalid samples. See the [app guide](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/bldc_tuner_windows/README_TH.md) and [v34 firmware guide](https://github.com/poommyroboticcamera-netizen/ESC-FOC-Drive-V02/blob/main/arduino_sixstep_releases/stm32f405_sixstep_app_v34/README.md).

Build: Electron 44.7.2 / electron-builder 26.15.3, Windows x64 portable. Source is in this repository. Real-board current accuracy and live motor timing have not yet been verified for v34.
