# Pin, ADC, current, voltage และ timing specification

อ้างอิง PDF custom หน้า 1–4 และ `FlyingProbeTesting.json` ใน Gerber_V02_2026-09-07.zip. ค่าที่เป็น proposed configuration ยังไม่ใช่ค่าที่ผ่าน oscilloscope test

## PWM / peripheral mapping

| Function | GPIO / LQFP64 pin | AF | Timer / channel | Net path | Logic |
|---|---|---|---|---|---|
| U high | PA8 /41 | AF1 | TIM1_CH1 | UH→R73→U_HIN→R20 pin2 | active-high candidate |
| U low | PB13 /34 | AF1 | TIM1_CH1N | UL→R76→U_LIN→R20 pin3 | active-high candidate |
| V high | PA9 /42 | AF1 | TIM1_CH2 | VH→R74→V_HIN→R39 pin2 | active-high candidate |
| V low | PB14 /35 | AF1 | TIM1_CH2N | VL→R77→V_LIN→R39 pin3 | active-high candidate |
| W high | PA10 /43 | AF1 | TIM1_CH3 | WH→R75→W_HIN→R40 pin2 | active-high candidate |
| W low | PB15 /36 | AF1 | TIM1_CH3N | WL→R78→W_LIN→R40 pin3 | active-high candidate |
| CAN RX/TX | PB8 /61,PB9 /62 | AF9 | CAN1 | RXD/TXD→U29 | MCP2551 +5V |
| USB DM/DP | PA11 /44,PA12 /45 | AF10 | OTG_FS | DP−/DP+→U28 | electrical USB QA required |
| PPM | PB6 /58 | AF2 | TIM4_CH1 | PPM→R87→H4 | pulse capture optional |
| SW3/SW4 | PC4 /24,PC5 /25 | GPIO | digital inputs | 10k pullup/100nF | active low |
| SWD | PA13 /46,PA14 /49 | AF0 | SWDIO/SWCLK | DIO/CLK→U19 | preserve debug pins |
| HSE | PH0 /5,PH1 /6 | oscillator | X4 8MHz | C77/78 18pF | clock validation required |
| BREAK candidate | PB12 /33 | AF1 if wired | TIM1_BKIN | NET_15 only MCU pad | **not connected** |
| UART candidate only | PB10 /29,PB11 /30 | AF7 | USART3 | NET_13/14 only MCU pads | not exposed connector |
| UART4 candidate only | PC10 /51,PC11 /52 | AF8 | UART4 | NET_21/22 only MCU pads | not exposed connector |
| Hall/ABI candidate only | PC6/7/8 | AF2/GPIO | TIM3 | isolated MCU pads | not fitted |

AF/channel capability checked against STM32F405 pin-function documentation and local STM32F4 headers. Candidate pin capability does not establish electrical routing. GPIOA/ GPIOB/TIM1 HAL can therefore stay separate from FOC math

## ADC map

ADC-unit column is a **selected schedule** among supported ADCs, not a dedicated hardware ownership of the pin. Full-scale convention below is VESC-compatible endpoint scale `Vadc = raw×VDDA/4095`; an ideal quantizer step is VDDA/4096, differing by 0.0244%. Use one convention consistently and calibrate actual gain/offset

| Signal | MCU pin | Selected ADC / channel | Scale / offset | Sampling |
|---|---|---|---|---|
| CURRENT_U | PC0 /8 | ADC1_IN10 | 20V/V ×1mΩ; per-channel measured zero | simultaneous rank1, PWM-valid low-side window |
| CURRENT_V | PC1 /9 | ADC2_IN11 | same; independent offset | simultaneous rank1 |
| CURRENT_W | PC2 /10 | ADC3_IN12 | same; independent offset | simultaneous rank1 |
| PHASE_U_V | PA0 /14 | ADC1_IN0 | (56k+2.2k)/2.2k =26.454545 | rank2 at known PWM phase |
| PHASE_V_V | PA1 /15 | ADC2_IN1 | same | rank2 |
| PHASE_W_V | PA2 /16 | ADC3_IN2 | same | rank2 |
| VBUS | none; PC3 currently unused | ADC3_IN13 possible only after hardware change | **unavailable; no valid scale** | physical independent divider required |
| TEMP_FET | none; PA3 unused | do not scan as sensor | unavailable | add NTC circuit |
| TEMP_MOTOR | none | PC4 is button | unavailable | disabled |
| ANALOG_INPUT | none | PA5/6 unused | unavailable | disabled |
| VDDA tracking | internal VREFINT | ADC1_IN17 | use calibration/reference method for actual MCU | slow acquisition, sufficient sample time |

Do not publish raw ADC values from unconnected channels as voltage/current/temperature measurements. Internal die temperature is not a MOSFET or motor temperature substitute

## Current-sense equations and polarity

Define `Ishunt > 0` from CUR1/2/3 into AGND. The PDF and exported netlist put IN+ at AGND, IN− at CUR. Thus:

```
Vout = Vref_amp - G × Rshunt × Ishunt
Vref_amp ≈ 3.3 × 1k/(1k+1k) = 1.65 V
Ishunt = (Vzero - raw×VDDA/4095)/(20×0.001)
```

During a valid low-side sample, define `Iphase > 0` from inverter terminal into motor, so `Iphase = -Ishunt` (neglecting switching displacement current). With calibrated zero `offset_raw`:

```
Iphase = (raw - offset_raw) × VDDA/(4095 × G × Rshunt)
       = (raw - offset_raw) × 0.0402930403 A/count   [VDDA=3.3,G=20,R=1mΩ]
```

This sign agrees with the schematic orientation; verify it by injecting known current in both directions through the shunt with gates held off. Do not invert merely because IN+ is at ground, and do not inherit six-step current-selector sign rules into a different FOC convention

| Quantity | Custom nominal | Reference nominal |
|---|---:|---:|
| Effective shunt | 0.001Ω | 0.0005/3Ω |
| Gain | 20 | 20 |
| Sensitivity | 20mV/A | 3.333333mV/A |
| A/count at VDDA3.3 | 0.04029304 | 0.24175824 |
| Ideal positive phase-current limit | +82.5A | +495A |
| Ideal negative phase-current limit | −82.5A | −495A |

These limits mean analog measurement range, not permissible phase/battery current. For example 10A positive phase current gives Vout1.85V, raw≈2295.68 for ideal zero2047.5. Positive shunt-to-ground10A gives1.45V, raw≈1799.32

INA181 datasheet p8 specifies output swing with RL=10k to GND and stated test conditions: high-side headroom30mV and low-side5mV at limits. Applying those headrooms to VS3.3V gives an **illustrative swing ceiling** −82.25A…+81.0A in the phase-current convention; characterize real rail/loading/temperature before treating it as guaranteed measurement range. Gain-error test range is Vout0.5…VS−0.5, corresponding to ±57.5A at3.3V; it is a characterization interval, not an automatically selected current limit

General clamp equations, allowing amplifier and ADC rails to differ:

```
Vlo = max(ADC_valid_min, amplifier_linear_min)
Vhi = min(ADC_valid_max, amplifier_linear_max)
Iphase_min = (Vlo - Vzero)/(G×Rshunt)
Iphase_max = (Vhi - Vzero)/(G×Rshunt)
```

Add rail-margin checks, out-of-range counts, stuck-channel diagnostics, calibrated offset bounds and RMS noise checks. Check `iu+iv+iw` only when all channels are valid; an unsampled shunt during high duty is not evidence of a failed sensor. Reconstruct at most one invalid current from two independently valid channels; two invalid channels → reject sample/reduce modulation or fault. Do not reconstruct through multiple ADC saturations

At shunt RMS100A, dissipation is `I²R=10W` per1mΩ; RMS50A gives2.5W. Actual shunt current is gated by low-side conduction, so calculate RMS from the real waveform. No resistance tolerance, TCR, package-power or Kelvin placement certification is available from values alone

The user removed output100nF; supply bypass C38/41/44 and input differential C37/40/43 must not be confused with it. REF1k/1k is a500Ω Thevenin source, not the current-output filter resistor. Assess INA181 REF drive/loading and measure actual zero; do not assume exactly2048 counts

## Phase voltage and VBUS

Custom R91/R96, R92/R95, R93/R94 =56k/2.2k:

```
Vphase = raw × (VDDA/4095) × 26.45454545
at VDDA3.3: 0.0213186813 V/count; endpoint full-scale =87.3V
Rthevenin =56k || 2.2k =2116.84Ω
```

87.3V is an ADC-divider mathematical limit. **IRFS7530 family VDS rating60V is lower**, and transient/ripple margins lower allowable bus voltage further. Voltage/current limits cannot be chosen from the divider limit alone. Verify acquisition settling with ADC input capacitance, source impedance and actual overshoot

Reference physical VBUS R12/R10=560k/21.5k:

```
ratio =27.04651163
VBUS_reference = raw ×0.0217957321 V/count at3.3V
endpoint =89.25348837V
```

Reference header uses3.34V and56k/2.2k →88.35818182V endpoint. It is numerically different from the reference physical divider; neither is a substitute for missing custom VBUS sensing

**Custom VBUS = TODO_HW_CONFIRMATION / hardware addition.** Do not use max(phase voltages), phase_U, average-phase or fixed24V to claim independent UV/OV protection. Such estimators depend on PWM state and cannot establish a valid measurement with all gates off

After direct sensing exists, choose limits in this order: device/cap/regulator/connector ratings and transient envelope → rated bus maximum → regen soft limit → hard OV cutoff → divider acquisition headroom. Worst-case resistor tolerance and minimum VDDA must keep ADC below its valid range. ADC absolute maximum is not a normal operating limit; design analog inputs within0…VDDA and check injection current constraints in ST datasheet. UV threshold must keep12V gate supply functional; numerical UV/OV/regen thresholds remain unset pending actual bus and supply tests

## Temperature conversion (future hardware only)

For NTC to ground and fixed pullup Rp: `Rntc=Rp×raw/(4095−raw)`.
For NTC to rail and fixed pulldown Rp: `Rntc=Rp×(4095/raw−1)`.
Then `T_C = 1/[1/(T25+273.15) + ln(Rntc/R25)/B] −273.15`.
Reject raw near0/4095 as sensor open/short according to orientation. R25/B and resistor orientation must come from fitted parts; no valid custom TEMP scale exists yet

## PWM synchronized acquisition design

The audited VESC FOC source uses **triple regular simultaneous ADC with DMA**, not injected-conversion ISR: ADC1 external trigger TIM2_CC2 falling edge, ADC2/3 follow multimode, ADC_CDR → DMA2_Stream4 channel0 → `mcpwm_foc_adc_int_handler`. Hardware setup also writes injected channel ranks, but this does not mean the FOC loop consumes JDR. Avoid copying six-step injected/median3 code from the old custom port without redoing timing

Existing board reference places phase-voltage rank1 and current rank2; DMA transfer-complete ISR. Proposed reduced target places all three currents at rank1; start current processing only after a coherent triple has transferred. If adding a half-transfer interrupt, prove the exact interleaved buffer layout and number of ranks; never enable HT just because it is faster

The old VESC default divides84MHz APB2 by2 → ADC42MHz and explicitly comments it exceeds36MHz. Proposed custom clock is APB2/4=21MHz, within the specified maximum. Example sample time15cycles +12 conversion cycles =1.286µs/rank. The entire acquisition aperture, not just its midpoint, must sit in the quiet valid window. Internal VREFINT needs its own longer acquisition plan; it must not silently consume the fast-loop budget or interrupt current acquisition unpredictably

Future carrier example: TIM1 clock168MHz, PSC0, center-aligned, ARR4200 →20kHz carrier. Software naming must distinguish carrier frequency, update-event rate and fast-loop execution rate. TIM2 runs at84MHz and requires the matching factor-of-two synchronization. Shadow CCR updates latch at a defined boundary; reject late updates and report missed deadlines. No timer PWM configuration is considered bench-verified yet

For each shunt:

```
t_valid_low >= t_dead + t_driver_switch + t_amp_settle + t_adc_acquire + t_margin
```

Choose trigger position from actual low-side gate/amp waveforms. At extreme duty, use two valid shunts to reconstruct the third; cap modulation if fewer than two are valid. There is no single schematic-derived minimum measurable duty: constraints depend on sector, zero-vector allocation, polarity and edge timing. For a symmetric window of `Tlow`, require the sample aperture after the leading blanking interval and before trailing turnoff guard. Upper duty limit, bootstrap refresh and sensing window must all hold simultaneously

## Gate-driver abstraction and dead-time

Proposed functions: `gate_driver_init`, `gate_driver_enable`, `gate_driver_disable`, `gate_driver_fault`, `gate_driver_clear_fault`. Return types must distinguish unavailable fault telemetry from healthy hardware. There is no SPI config/enable/fault pin in the schematic. `enable` must refuse while mandatory hardware capabilities are absent. `disable` must clear TIM1 MOE/CCER and explicitly force all six GPIOs low; setting CCR=0 alone can leave a complementary low-side MOSFET ON

EG3112 family documents active-high HIN/LIN and interlock; exact fitted EG3112D characteristics remain to be checked. Interlock is not shunt over-current detection. 12V supply and bootstrap10µF are nominal. Bootstrap droop budget: `ΔV=(Qg + Cgs_ext×ΔVgate + Iqbs×Ton + leakage×Ton)/Cboot_eff`; include capacitor DC-bias derating and diode drop. No100% high-side duty allowed without proving refresh

Cgs_ext22nF stores264nC at12V, comparable with MOSFET gate charge; it cannot be ignored. IRFS7530 vs IRFS7530-7P have different Qg; use exact suffix. External10Ω plus driver/internal gate resistance gives nontrivial turnoff delay. Reference660ns is only a starting comparison, not a released custom setting

TIM1 BDTR DTG encoding with CKD=DIV1, tDTS=1/168MHz: codes0…127 → N×tDTS;128…191 →(64+Nlow6)×2tDTS;192…223 →(32+Nlow5)×8tDTS;224…255 →(32+Nlow5)×16tDTS. Ceiling660ns →111ticks=660.714ns; ceiling1µs →code148=1µs. Final selection requires measured high-/low-side VGS non-overlap at worst voltage/current/temperature, with AOE off and explicit software rearm after a fault

## Sources

- [STM32F405 datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf), pin AF tables and ADC electrical limits
- [STM32F405 documentation / RM0090](https://www.st.com/en/microcontrollers-microprocessors/stm32f405-415/documentation.html), TIM/ADC/DMA register behavior
- [TI INA181 datasheet](https://www.ti.com/lit/ds/symlink/ina181.pdf), gain, output swing/load and reference behavior
- [EGmicro driver table](https://www.egmicro.com/products/filter_drive?category_id=30&lang=en), EG3112 family behavior; exact driver suffix needs verification
- [IRFS7530](https://www.infineon.com/part/IRFS7530), [IRFS7530-7P](https://www.infineon.com/part/IRFS7530-7P), 60V family and different packages
- Local source: `hwconf/makerbase/75_100/hw_mksesc_75_100_core.{c,h}`, `hwconf/mcuconf.h`, `motor/mcpwm_foc.c`, `conf_general.h`. Exact source hashes are recorded in the evidence manifest
