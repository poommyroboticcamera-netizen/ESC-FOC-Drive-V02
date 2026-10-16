# STM32F405 BLDC firmware v33-proto1 for SAT motor pilot

This sketch is a separate copy based on `sixstepf405_current` `v32-current-ref2`. It does not replace the original sketch in the Desktop project or any files in `bldc_tuner_windows` or `MotorPilot_F405Dual`.

## Protocol additions

- USB CDC, 115200 baud, 8-N-1, newline-terminated ASCII commands.
- `HELLO` returns firmware/protocol identity and declared limitations.
- `HELLO` lists supported controls and phase telemetry, plus VESC-style live values that this board does not provide. `STATUS` contains the supported named fields. The unavailable list includes motor/battery current in amperes, bus voltage, mechanical RPM, temperatures, energy counters and tachometer; these are not inferred or fabricated.
- `HEARTBEAT` renews a 1500 ms application lease. `ARM` and `RUN` are rejected until a fresh heartbeat has been received.
- While `ARMED_IDLE`, an expired lease returns the controller to `DISARMED`. During `ALIGN`, `STARTUP`, or `CLOSED_LOOP`, an expired lease turns the gate outputs off and latches `APP_TIMEOUT`; it does not restart the motor. `ARM` is an explicit user action to clear a fault.
- `STATUS` returns one fixed-format `OK STATUS` line with named fields and units. `DIAG` includes this status before diagnostic details.
- `STREAM ON [interval_ms]` starts a phase sample stream (default 100 ms; allowed 100–1000 ms). `STREAM OFF` stops it. Each complete `OK PHASE` line is one time-aligned U/V/W ADC sample with a common `t_us`, validity flag, and sample age. A sample is skipped if USB transmit space is insufficient; the motor loop never waits for telemetry.
- `OK PHASE` now includes `f=U|V|W`, the floating phase that corresponds to its U/V/W raw ADC sample. The PC can compute `3*floating - U - V - W` in ADC differential counts. Driven phase terminal readings must not be called back EMF; neither this derived count nor the raw phase ADC is a calibrated voltage.
- `SCOPE CAPTURE` is accepted only in `CLOSED_LOOP`. It copies at most 512 PWM-ON raw U/V/W ADC conversions, every second eligible conversion, to a fixed RAM snapshot with actual microsecond timestamps and the floating phase/step. It stops collecting at 512 samples, after 2 seconds, or if drive stops. It does not render, filter, or retain multiple captures. `SCOPE STATUS` reports `CAPTURING`, `READY`, or `EMPTY`. After a separate STOP, `SCOPE READ` sends `OK SCOPE SAMPLE` lines and `OK SCOPE END`; each line enters USB only if the whole line fits the queue. `SCOPE CLEAR` discards the snapshot while stopped. USB reading never runs during active drive, and the normal heartbeat and fault checks remain on the board.
- `HELLO` and requested `STATUS` are buffered as complete protocol lines, then copied into the USB transmit queue only when it has space. They do not spin waiting for the host. While a long reply is pending, regular phase frames are skipped; motor and current-monitor supervision continue each loop. On a stalled host, the application heartbeat expires and the firmware stops active drive. Diagnostic text from optional current calibration/scan paths remains outside this telemetry queue and has not been qualified for live plotting during RUN.
- The board sends phase samples at up to 10 Hz when the app enables `STREAM ON 100`; the app requests `STATUS` at about 2 Hz. The board stores no graph history and does no chart scaling or statistics. The PC owns the plot buffers and rendering. Dropped phase frames are not reconstructed into synthetic measurements.
- `bldc_tuner_windows` contains the matching **SAT motor pilot** Web Serial app. Its handshake checks `fw=v33-proto1`, `protocol=1`, and `current_protection=OFF`; it sends `HEARTBEAT`, polls `STATUS`, and subscribes to `OK PHASE` at 100 ms. The app shows raw phase counts, not calibrated phase voltage or current.
- Accepted/rejected primary control commands use `OK`/`ERR` replies. Board-side validation remains authoritative.
- Startup tuning is available only while the drive is stopped: `STARTPERIOD 12000..30000` sets the initial forced sector duration in microseconds; `RAMP 150..300` sets the divisor used to reduce the forced sector duration after each eligible startup step (smaller means faster acceleration). Defaults are 20000 us and 200; the previous fixed ramp divisor was 250. `STATUS` reports `startup_initial_period_us` and `startup_ramp_divisor`. ALIGN remains 200 ms, startup duty remains capped at 5%, BEMF lock and fault checks remain unchanged. These settings are RAM-only and return to defaults on reset; faster values need bench validation with the intended motor and supply.
- `RESPONSE 10..30` changes the stopped-only duty slew in percentage points per second; default 20 versus the previous fixed 10. It still advances only 0.1% per scheduled tick, never catches up with a jump after a delayed loop, and keeps the startup 5% duty cap. `STATUS` reports `duty_ramp_pct_per_s`. This is not current control or overcurrent protection.

The telemetry resembles the live drive status in VESC Tool where measurements overlap: state/fault, duty, timing and status plus the phase ADC traces. It is not a VESC protocol implementation. In `STATUS` and `PHASE`, periods and timestamps/age use microseconds (`*_us`), phase ADC values use raw counts (`*_adc_counts`), duty target uses percent, and `*_pct_tenths` values use tenths of a percent (for example, `50` means `5.0%`). `phase_valid` qualifies phase data; `phase_age_us` reports its age. The three ADC fields in one `PHASE` frame are intended as separate U/V/W graph series against the shared `t_us` time axis. These ADC counts are not phase voltages until the divider and ADC scaling are verified. A heartbeat age of `4294967295` means no heartbeat has been received since boot.

VESC Tool exposes additional fields such as input voltage, motor/input current, RPM, temperatures, energy and tachometer in its live values model. This firmware reports those as unavailable in `HELLO` because the present hardware/sketch does not provide calibrated or qualified values for them. The current monitor remains diagnostic-only and must not be relabeled as amperes or used for current trip control.

Example:

```text
HELLO
OK HELLO fw=v33-proto1 protocol=1 board=STM32F405 ...
HEARTBEAT
OK HEARTBEAT lease_ms=1500
STATUS
OK STATUS fw=v33-proto1 protocol=1 state=DISARMED fault=none ... phase_u_adc_counts=... current_protection=OFF current_data=UNVERIFIED
STREAM ON 100
OK STREAM kind=phase state=ON interval_ms=100
OK PHASE t_us=123456 u_adc_counts=2010 v_adc_counts=2022 w_adc_counts=1998 phase_valid=1 phase_age_us=25
```

The controller behavior, startup sequence, BEMF checks, supported controls, current monitor and protection settings are inherited from v32-current-ref2. Current data is explicitly marked `UNVERIFIED`; current protection remains OFF. Do not infer confirmed amperes or use these diagnostics for an automatic current trip.

## Build and hardware status

Select Generic STM32F4 / Generic F405RGTx with USB CDC as in the source README retained at `README_SOURCE_v32-current-ref2.md`. Open `stm32f405_sixstep_app_v33.ino`, compile it before flashing, and use only with SAT motor pilot. No board has been tested with v33-proto1 yet. In particular, the 1500 ms heartbeat timeout and the physical gate-off behavior on timeout, USB loss, reset, or IWDG reset require bench verification before relying on them with a motor.

Test first with the motor disconnected and a current-limited supply. Confirm `HELLO`, `STATUS`, heartbeat expiry in `ARMED_IDLE`, `APP_TIMEOUT` while running under a controlled bench setup, gate waveforms, and that a timeout never automatically restarts the motor. Keep the physical DC bus cutoff available. This firmware does not make the existing hardware current protection verified.

The Scope capture path is compiled but not bench validated. Verify that recording a 512-sample burst does not change commutation timing, ADC validity, or fault response before using it as a measurement. At low duty, PWM pulse skipping makes sample spacing uneven; use the transmitted `t_us` values. A 10 Hz phase stream remains the regular low-bandwidth view. A continuous three-phase back-EMF waveform while phases are driven cannot be obtained from these raw terminal samples alone.
