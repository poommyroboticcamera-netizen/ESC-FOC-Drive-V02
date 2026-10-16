#include <Arduino.h>
#include <HardwareTimer.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * STM32F405RGT6 / EG2132 six-step sensorless bench firmware.
 * HIN U/V/W: PA8/9/10 (TIM1 PWM, active HIGH).
 * LIN U/V/W: PB13/14/15 (GPIO, active HIGH).
 * Phase dividers U/V/W: PA0/1/2 = ADC1/2/3 channel 0/1/2.
 * PC4/PC5 switches are NOT used. PC0/1/2 current monitor is diagnostic only.
 * Stop with Serial STOP or remove DC-bus power. v33-proto1 also requires a
 * fresh application HEARTBEAT; loss during drive latches outputs OFF.
 *
 * TIM1 CH4 (internal PWM2, no output pin) starts all three injected ADC
 * conversions at the SAME instant during PWM ON. Never call analogRead:
 * it would reconfigure the ADC peripheral used here.
 * ADC clock = APB2/4. 15 sample + 12 conversion cycles at 21 MHz = ~1.29 us.
 * Trigger at half of the ON pulse, minimum duty 16% (8 us at 20 kHz).
 * Below 16% average, pulse skipping preserves the 8 us ADC-readable width.
 * At >=16%, fractional duty uses timer ticks with NO density-skipped cycles.
 * CCR preload loads both pulse width and ADC trigger at timer rollover.
 * MOE is a safety/commutation gate, never a per-period software pulse gate.
 * TIM2 one-shot: commutation deadline, then a 10 us all-OFF interval.
 * ADC and TIM2 IRQs share priority 2 and cannot preempt one another.
 * No Serial I/O or waiting in an ISR. Watchdog reset returns to DISARMED.
 *
 * Bring-up firmware, not a guaranteed production ESC. Use external current
 * limiting: BEMF loss detection does not protect against electrical shorts.
 * Phase dividers must have matched ratios and suitable RC settling.
 * Algorithm: alignment -> forced ramp -> 6 consecutive timing-qualified
 * crossings -> phase-aligned handover -> closed loop, 30 degree delay.
 * References: ST RM0090 ADC injected trigger table; ST AN4220.
 */
#if !defined(STM32F405xx)
#error "Select Generic STM32F4 series / Generic F405RGTx."
#endif
#if !defined(USBCON) || !defined(USBD_USE_CDC)
#error "Tools > USB support: select CDC (generic Serial supersede U(S)ART)."
#endif

const uint32_t PIN_U_HIN = pinNametoDigitalPin(PA_8);
const uint32_t PIN_V_HIN = pinNametoDigitalPin(PA_9);
const uint32_t PIN_W_HIN = pinNametoDigitalPin(PA_10);
const uint32_t PIN_U_LIN = pinNametoDigitalPin(PB_13);
const uint32_t PIN_V_LIN = pinNametoDigitalPin(PB_14);
const uint32_t PIN_W_LIN = pinNametoDigitalPin(PB_15);

constexpr uint32_t PWM_FREQUENCY_HZ = 20000; // more RUN samples; preserve 8 us minimum ON
constexpr uint32_t GATE_BLANK_US = 10;
constexpr uint32_t ALIGN_US = 200000; // match user-confirmed working v32 startup profile
constexpr uint16_t ALIGN_START_PERMILLE = 20; // 2% average drive
constexpr uint16_t ALIGN_END_PERMILLE = 50;   // 5% average before startup ramp
constexpr uint16_t DEFAULT_DUTY_RAMP_PCT_PER_S = 20;
constexpr uint16_t MIN_DUTY_RAMP_PCT_PER_S = 10;
constexpr uint16_t MAX_DUTY_RAMP_PCT_PER_S = 30;
constexpr uint32_t MIN_CROSS_GAP_US = 150;
constexpr uint32_t PWM_SAMPLE_GAP_US = (9000000UL + 4UL * PWM_FREQUENCY_HZ - 1UL) /
    (4UL * PWM_FREQUENCY_HZ); // ceil(2.25 carrier periods), no early integer truncation
constexpr uint16_t STARTUP_MAX_PERMILLE = 50; // match tested v32; NOT a current limit
constexpr uint16_t STARTUP_RAMP_READY_PERMILLE = 40;
constexpr uint32_t START_PERIOD_US = 20000;
constexpr uint32_t MIN_START_PERIOD_US = 12000;
constexpr uint32_t MAX_START_PERIOD_US = 30000;
constexpr uint16_t DEFAULT_STARTUP_RAMP_DIVISOR = 200;
constexpr uint16_t MIN_STARTUP_RAMP_DIVISOR = 150; // lower divisor accelerates faster
constexpr uint16_t MAX_STARTUP_RAMP_DIVISOR = 300;
constexpr uint32_t DEFAULT_TARGET_US = 3000;
constexpr uint32_t MIN_PERIOD_US = 600; // v28 experimental high-speed floor; NOT a motor RPM rating
constexpr uint32_t HARD_FAST_PERIOD_US = 500; // emergency bound, NOT a supported operating target
constexpr uint8_t SPEED_GUARD_CYCLES = 2; // consecutive full electrical cycles below 600 us
constexpr uint32_t MAX_TARGET_US = 12000;
constexpr uint32_t MAX_MEASURED_US = 30000;
constexpr uint32_t STARTUP_TIMEOUT_US = 10000000;
constexpr uint32_t ADC_TIMEOUT_US = 3000;
constexpr uint8_t MIN_DUTY = 5;
constexpr uint8_t MIN_PULSE_DUTY = 16; // 8 us ON at 20 kHz, independent of average duty
constexpr uint8_t MAX_DUTY = 80;
constexpr uint8_t DEFAULT_DUTY = 10;
constexpr uint8_t LOCK_CROSSES = 6; // one full electrical revolution, consecutive
constexpr uint32_t APP_HEARTBEAT_TIMEOUT_US = 1500000UL;
constexpr uint32_t APP_HEARTBEAT_TIMEOUT_MS = APP_HEARTBEAT_TIMEOUT_US / 1000UL;
constexpr uint16_t STATUS_STREAM_DEFAULT_MS = 100;
constexpr uint16_t STATUS_STREAM_MIN_MS = 100;
constexpr uint16_t STATUS_STREAM_MAX_MS = 1000;
constexpr uint32_t CLOSED_LOOP_SETTLE_STEPS = 12;
constexpr uint16_t LOW_CLAMP_FLOOR_COUNTS = 32;
constexpr uint16_t LOW_CLAMP_DIVISOR = 8; // reject LOW above 1/8 of driven HIGH
constexpr uint32_t PERIOD_SLEW_DIVISOR = 20; // max 5% per accepted crossing
constexpr uint8_t EDGE_CONFIRMATIONS = 3;
constexpr uint32_t FAST_AFTER_SECTOR_US = 1000;
constexpr uint32_t MIN_COMM_LEAD_US = 16; // reserve scheduling time; do not commutate an expired edge
constexpr uint32_t FAST_SECTOR_US = 2000;
constexpr uint16_t MIN_DRIVEN_SPAN = 160;
constexpr uint16_t SIGNAL_BAD_LIMIT = 400;
constexpr uint32_t LIN_MASK = (1UL << 13) | (1UL << 14) | (1UL << 15);

enum class State : uint8_t { DISARMED, ARMED_IDLE, ALIGN, STARTUP, CLOSED_LOOP, FAULT };
enum class Fault : uint8_t { NONE, ADC_TIMEOUT, PHASE_SIGNAL, ADC_SATURATED, STARTUP_TIMEOUT, BEMF_LOST, SPEED_LIMIT, COMM_LATE, CLOCK, CURRENT_MONITOR, APP_TIMEOUT };
enum class TimerStage : uint8_t { MAKE, ADVANCE };

// high/low/floating indices, U=0 V=1 W=2. Falling/rising for forward rotation.
struct Step { uint8_t high, low, floating; bool rising; };
constexpr Step STEPS[6] = {
  {0, 1, 2, false}, {0, 2, 1, true}, {1, 2, 0, false},
  {1, 0, 2, true}, {2, 0, 1, false}, {2, 1, 0, true}
};
constexpr int32_t phaseError(uint16_t high, uint16_t low, uint16_t floating) {
  // Equivalent to floating - mean(U,V,W), scaled by 3. No divide in ISR.
  return 2 * static_cast<int32_t>(floating) - high - low;
}
static_assert(phaseError(1000, 0, 500) == 0, "neutral");
static_assert(phaseError(1000, 0, 600) > 0, "rising");
static_assert(phaseError(1000, 0, 400) < 0, "falling");
static_assert(phaseError(1000, 100, 550) == 0, "common mode");
static_assert(STEPS[0].floating == 2 && STEPS[5].floating == 0, "phase table");

HardwareTimer pwmTimer(TIM1);
HardwareTimer commTimer(TIM2);
volatile State state = State::DISARMED;
volatile Fault fault = Fault::NONE;
volatile TimerStage timerStage = TimerStage::ADVANCE;
volatile bool forwardDirection = true;
volatile uint8_t dutyPercent = DEFAULT_DUTY;
volatile uint16_t effectiveDutyPermille = 0;
volatile uint16_t physicalDutyPermille = MIN_PULSE_DUTY * 10;
volatile bool pwmDriveEnabled = false;
volatile bool pwmPulseEnabled = false;
volatile bool pwmPreparedPulse = false;
volatile uint32_t driveStartedUs = 0;
volatile uint32_t lastDutyRampUs = 0;
uint16_t pulseAccumulator = 0;
volatile uint8_t activeStep = 0;
volatile uint8_t pendingStep = 0;
volatile uint32_t targetPeriodUs = DEFAULT_TARGET_US;
volatile uint32_t initialPeriodUs = START_PERIOD_US;
volatile uint16_t startupRampDivisor = DEFAULT_STARTUP_RAMP_DIVISOR;
volatile uint16_t dutyRampPctPerS = DEFAULT_DUTY_RAMP_PCT_PER_S;
volatile uint32_t dutyRampTickUs = 100000UL / DEFAULT_DUTY_RAMP_PCT_PER_S;
volatile uint32_t forcedPeriodUs = START_PERIOD_US;
volatile uint32_t measuredPeriodUs = START_PERIOD_US;
volatile uint32_t lastCommutationUs = 0;
volatile uint32_t blankUntilUs = 0;
volatile uint32_t startupAtUs = 0;
volatile uint32_t sectorCount = 0;
volatile uint32_t adcSamples = 0;
volatile uint32_t lastAdcUs = 0;
volatile uint16_t latestRaw[3] = {0, 0, 0};
volatile uint8_t latestFloatingIndex = 0;
volatile uint16_t latestNeutral = 0;
volatile int32_t latestError = 0;
volatile bool latestSignalValid = false;
volatile uint16_t badSignalCount = 0;
volatile uint8_t lockCount = 0;
volatile uint8_t peakLockCount = 0;
volatile uint32_t lowClampRejected = 0;
volatile uint16_t lowRejectedByStep[6] = {};
volatile uint16_t crossingsByStep[6] = {};
volatile uint16_t centeredByStep[6] = {};
volatile uint16_t crossingPosition[6] = {}; // permille of sector period
volatile bool crossedThisStep = false;
volatile bool previousCrossValid = false;
volatile uint32_t previousCrossUs = 0;
volatile uint8_t previousCrossStep = 0;
volatile uint32_t crossingCount = 0;
volatile uint32_t rejectedCrossings = 0;
volatile uint32_t skippedAdcSets = 0;
volatile bool lockNotice = false;
volatile uint32_t closedLoopSteps = 0;
volatile uint32_t invalidPhaseSamples = 0;
bool sectorLateCandidate = false;
uint32_t sectorValidSamples = 0;
uint32_t sectorInvalidSamples = 0;
int32_t sectorErrorMin = INT32_MAX, sectorErrorMax = INT32_MIN;
uint8_t sectorRejectReason = 0; // 1 early, 2 late, 3 interval/order
uint32_t sectorEdgeIntervalUs = 0, sectorEdgeExpectedUs = 0;
bool sectorEdgeSequential = false;
uint32_t sectorConfirmDelayUs = 0;
int32_t sectorCommLeadUs = 0;
volatile int32_t minimumCommLeadUs = INT32_MAX;
volatile uint32_t lateConfirmedCrossings = 0;
// RUN speed guard is independent of the per-edge commutation estimate.
uint32_t speedCycleSumUs = 0;
uint8_t speedCycleSamples = 0, speedFastCycles = 0;
uint32_t speedCycleAverageUs = 0;
uint8_t consecutiveInvalidSamples = 0;
uint32_t sectorNegativeSamples = 0, sectorFirstValidIntoUs = 0;
uint8_t sectorPeakBeforeCount = 0;
// Only accessed in serialized ADC/TIM2 ISRs or with interrupts masked.
uint16_t adcHistory[3][3] = {};
uint32_t adcHistoryTime[3] = {};
uint8_t historySlot = 0;
uint8_t historyCount = 0;
bool seenBefore = false;
uint8_t beforeCount = 0;
uint8_t afterCount = 0;
bool candidateValid = false;
uint32_t candidateUs = 0;
bool previousErrorValid = false;
int32_t previousError = 0;
uint32_t previousErrorUs = 0;

char commandBuffer[64];
uint8_t commandLength = 0;
bool commandOverflow = false;
bool peripheralsReady = false;
bool timerInitOK = false;
bool watchdogInitOK = false;
uint32_t motorMicros() { return TIM5->CNT; }
bool configureMotorClock() {
  __HAL_RCC_TIM5_CLK_ENABLE();
  const uint32_t hz = commTimer.getTimerClkFreq();
  if (hz < 1000000 || hz % 1000000 != 0) return false;
  TIM5->CR1 = 0;
  TIM5->PSC = hz / 1000000 - 1;
  TIM5->ARR = 0xFFFFFFFFUL;
  TIM5->EGR = TIM_EGR_UG;
  TIM5->CNT = 0;
  TIM5->CR1 = TIM_CR1_CEN;
  return true;
}
Fault reportedFault = Fault::NONE;
uint32_t lastAppHeartbeatUs = 0;
bool appHeartbeatSeen = false;
bool phaseStreamEnabled = false;
uint16_t phaseStreamIntervalMs = STATUS_STREAM_DEFAULT_MS;
uint32_t nextPhaseStreamUs = 0;
char phaseStreamBuffer[128];
// One bounded raw snapshot. The PC owns waveform rendering and history.
constexpr uint16_t SCOPE_MAX_SAMPLES = 512;
constexpr uint16_t SCOPE_LIVE_SAMPLES = 256;
constexpr uint32_t SCOPE_LIVE_FRAME_INTERVAL_US = 300000UL;
constexpr uint32_t SCOPE_MAX_CAPTURE_US = 2000000UL;
struct ScopeSample { uint32_t tUs; uint16_t u, v, w; uint8_t floating, step; };
ScopeSample scopeSamples[SCOPE_MAX_SAMPLES];
volatile uint16_t scopeCount = 0;
volatile bool scopeCapturing = false;
volatile bool scopeReady = false;
volatile uint8_t scopeDecimation = 0;
uint32_t scopeStartedUs = 0;
bool scopeSending = false;
volatile bool scopeLive = false;
bool scopeLiveFrame = false, scopeFrameBegin = false;
uint32_t scopeLiveNextUs = 0;
uint16_t scopeSendIndex = 0;
uint32_t scopeSendStartedUs = 0;
char scopeLine[128];
char telemetryReplyBuffer[1200];
size_t telemetryReplyLength = 0;
size_t telemetryReplyOffset = 0;

uint32_t enterCritical() {
  const uint32_t mask = __get_PRIMASK();
  __disable_irq();
  return mask;
}
void leaveCritical(uint32_t mask) { __set_PRIMASK(mask); }
bool due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
bool motorActive(State s) {
  return s == State::ALIGN || s == State::STARTUP ||
         s == State::CLOSED_LOOP;
}
bool appHeartbeatFresh(uint32_t now) {
  return appHeartbeatSeen && now - lastAppHeartbeatUs <= APP_HEARTBEAT_TIMEOUT_US;
}
uint8_t followingStep(uint8_t s) {
  return forwardDirection ? (s + 1) % 6 : (s + 5) % 6;
}

void gatesOff() {
  pwmDriveEnabled = false;
  pwmPulseEnabled = false;
  pwmPreparedPulse = false;
  TIM1->BDTR &= ~TIM_BDTR_MOE;
  TIM1->CCR1 = 0; TIM1->CCR2 = 0; TIM1->CCR3 = 0;
  GPIOB->BSRR = LIN_MASK << 16;
}
void disableAdcTrigger() {
  ADC1->CR2 &= ~ADC_CR2_JEXTEN;
  ADC2->CR2 &= ~ADC_CR2_JEXTEN;
  ADC3->CR2 &= ~ADC_CR2_JEXTEN;
}
void enableAdcTrigger() {
  ADC1->SR = 0; ADC2->SR = 0; ADC3->SR = 0;
  ADC1->CR2 |= ADC_CR2_JEXTEN_0;
  ADC2->CR2 |= ADC_CR2_JEXTEN_0;
  ADC3->CR2 |= ADC_CR2_JEXTEN_0;
}
bool bemfSectorEligible() {
  return state == State::CLOSED_LOOP || (state == State::STARTUP && sectorCount >= 12);
}
void stopNow(Fault reason) {
  gatesOff();
  disableAdcTrigger();
  TIM2->CR1 &= ~TIM_CR1_CEN;
  TIM2->SR = 0;
  fault = !peripheralsReady ? Fault::CLOCK : reason;
  state = fault == Fault::NONE ? State::DISARMED : State::FAULT;
}

void scheduleAfter(uint32_t delayUs, TimerStage stage) {
  // TIM2 is configured at 1 MHz. URS prevents UG from producing an IRQ.
  if (delayUs < 4) delayUs = 4;
  TIM2->CR1 &= ~TIM_CR1_CEN;
  timerStage = stage;
  TIM2->ARR = delayUs - 1;
  TIM2->CNT = 0;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0;
  TIM2->CR1 |= TIM_CR1_CEN;
}
void scheduleAt(uint32_t deadline) {
  const uint32_t now = motorMicros();
  scheduleAfter(due(now, deadline) ? 4 : deadline - now, TimerStage::ADVANCE);
}
void resetSectorDetector() {
  sectorValidSamples = 0; sectorInvalidSamples = 0;
  sectorErrorMin = INT32_MAX; sectorErrorMax = INT32_MIN;
  sectorRejectReason = 0;
  sectorEdgeIntervalUs = 0; sectorEdgeExpectedUs = 0;
  sectorEdgeSequential = false;
  sectorConfirmDelayUs = 0; sectorCommLeadUs = 0;
  sectorNegativeSamples = 0; sectorFirstValidIntoUs = 0; sectorPeakBeforeCount = 0;
  sectorLateCandidate = false;
  consecutiveInvalidSamples = 0;
  historySlot = 0; historyCount = 0;
  seenBefore = false; beforeCount = 0; afterCount = 0;
  candidateValid = false; previousErrorValid = false;
  crossedThisStep = false;
}
volatile uint32_t currentPulseEpoch = 0, currentOnTicks = 0;
uint32_t currentPreparedOnTicks = 0;
void onPwmPeriod() {
  if (!pwmDriveEnabled) return;
  // Rollover has ALREADY loaded the preceding preload into active CCRs.
  // Tag that hardware pulse first; the rest of this ISR prepares NEXT cycle.
  pwmPulseEnabled = pwmPreparedPulse;
  currentOnTicks = currentPreparedOnTicks;
  ++currentPulseEpoch;
  const uint16_t requested = effectiveDutyPermille;
  const uint16_t pulseDuty = max(static_cast<uint16_t>(MIN_PULSE_DUTY * 10), requested);
  physicalDutyPermille = pulseDuty;
  const uint32_t onTicks = (TIM1->ARR + 1UL) * pulseDuty / 1000UL;
  const Step &step = STEPS[activeStep];
  // Never toggle MOE here:
  // doing that after interrupt latency shortens the pulse's leading edge,
  // whereas CC4 still triggers at the original timer-relative midpoint.
  if (requested >= MIN_PULSE_DUTY * 10) {
    // Do not round 52.2% to 53% and skip cycles. That removes a BEMF
    // acquisition during the short pre-crossing window at high speed.
    pwmPreparedPulse = true;
    pulseAccumulator = 0;
  } else {
    // Low-duty startup retains minimum-width pulses and density control.
    pulseAccumulator += requested;
    pwmPreparedPulse = pulseAccumulator >= pulseDuty;
    if (pwmPreparedPulse) pulseAccumulator -= pulseDuty;
  }
  const uint32_t nextOnTicks = pwmPreparedPulse ? onTicks : 0;
  currentPreparedOnTicks = nextOnTicks;
  TIM1->CCR1 = step.high == 0 ? nextOnTicks : 0;
  TIM1->CCR2 = step.high == 1 ? nextOnTicks : 0;
  TIM1->CCR3 = step.high == 2 ? nextOnTicks : 0;
  TIM1->CCR4 = onTicks / 2UL; // ADC heartbeat also runs in skipped cycles
}
void beginStep(uint8_t step) {
  gatesOff();
  disableAdcTrigger();
  pendingStep = step;
  scheduleAfter(GATE_BLANK_US, TimerStage::MAKE);
}
void makeStep() {
  const uint8_t s = pendingStep;
  if (s >= 6) { stopNow(Fault::PHASE_SIGNAL); return; }
  // beginStep has already held all gates OFF for GATE_BLANK_US.
  // Stop the carrier while loading the new phase; UG must not create
  // a software-update callback that consumes a pulse-density decision.
  TIM1->BDTR &= ~TIM_BDTR_MOE;
  TIM1->CR1 &= ~TIM_CR1_CEN;
  activeStep = s;
  resetSectorDetector();
  const Step &step = STEPS[s];
  const uint32_t ticks = TIM1->ARR + 1;
  physicalDutyPermille = max(static_cast<uint16_t>(MIN_PULSE_DUTY * 10),
                             static_cast<uint16_t>(effectiveDutyPermille));
  const uint32_t onTicks = ticks * physicalDutyPermille / 1000UL;
  // Continuous-width drive can start its first complete pulse now. Low-duty
  // density startup keeps the original OFF first cycle and accumulator.
  const bool firstPulse = effectiveDutyPermille >= MIN_PULSE_DUTY * 10;
  const uint32_t firstOnTicks = firstPulse ? onTicks : 0;
  currentOnTicks = firstOnTicks; currentPreparedOnTicks = firstOnTicks;
  ++currentPulseEpoch;
  TIM1->CCR1 = step.high == 0 ? firstOnTicks : 0;
  TIM1->CCR2 = step.high == 1 ? firstOnTicks : 0;
  TIM1->CCR3 = step.high == 2 ? firstOnTicks : 0;
  // Sample halfway through the high-side pulse; all ADCs convert together.
  TIM1->CCR4 = onTicks / 2;
  TIM1->CNT = 0;
  TIM1->EGR = TIM_EGR_UG;
  TIM1->SR = 0; // discard UG flags; only real carrier rollovers run the ISR
  GPIOB->BSRR = 1UL << (13 + step.low);
  lastCommutationUs = motorMicros();
  const uint32_t period = state == State::CLOSED_LOOP ? measuredPeriodUs : forcedPeriodUs;
  // RUN can lose the short pre-crossing side after a late preceding edge.
  // Match the known-working v32 startup blanking; keep the faster RUN window.
  const uint32_t blank = state == State::CLOSED_LOOP
      ? constrain(period / 16UL, 80UL, 500UL)
      : constrain(period / 8UL, 150UL, 1000UL);
  blankUntilUs = lastCommutationUs + blank;
  lastAdcUs = lastCommutationUs;
  enableAdcTrigger();
  pulseAccumulator = 0;
  pwmDriveEnabled = true;
  pwmPulseEnabled = firstPulse;
  pwmPreparedPulse = firstPulse;
  // CNT is stopped at zero, with new CCR1..4 loaded together by UG.
  // Enable the gate and immediately start counting: no arbitrary mid-cycle
  // pulse, and no extra empty carrier before the new phase can be sampled.
  TIM1->BDTR |= TIM_BDTR_MOE;
  TIM1->CR1 |= TIM_CR1_CEN;
  if (state == State::ALIGN) scheduleAfter(ALIGN_US, TimerStage::ADVANCE);
  else if (state == State::CLOSED_LOOP) {
    // Deadline becomes normal commutation once a crossing arrives.
    scheduleAfter(measuredPeriodUs * 3UL / 2UL, TimerStage::ADVANCE);
  } else scheduleAfter(forcedPeriodUs, TimerStage::ADVANCE);
}
void onCommutationTimer() {
  if (!motorActive(state)) return;
  if (timerStage == TimerStage::MAKE) { makeStep(); return; }
  if (state == State::ALIGN) {
    state = State::STARTUP;
    startupAtUs = motorMicros();
    lastDutyRampUs = startupAtUs;
  } else if (state == State::CLOSED_LOOP) {
    if (!crossedThisStep) { stopNow(Fault::BEMF_LOST); return; }
    ++closedLoopSteps;
  } else if (state == State::STARTUP) {
    if (!crossedThisStep) { lockCount = 0; previousCrossValid = false; }
    ++sectorCount;
    const uint32_t decrement = max(10UL, forcedPeriodUs / startupRampDivisor);
    const bool signalReady = sectorValidSamples >= 4 &&
        sectorInvalidSamples * 3UL <= sectorValidSamples;
    // Preserve the known-working v32 forced-ramp policy during startup.
    if (!crossedThisStep && !sectorLateCandidate && signalReady &&
        effectiveDutyPermille >= STARTUP_RAMP_READY_PERMILLE &&
        forcedPeriodUs > targetPeriodUs) {
      forcedPeriodUs = max(static_cast<uint32_t>(targetPeriodUs),
                           static_cast<uint32_t>(forcedPeriodUs) - decrement);
    }
    const uint32_t now = motorMicros();
    if (now - startupAtUs > STARTUP_TIMEOUT_US) {
      stopNow(Fault::STARTUP_TIMEOUT); return;
    }
  }
  beginStep(followingStep(activeStep));
}
uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
  if (a > b) { const uint16_t t = a; a = b; b = t; }
  if (b > c) { const uint16_t t = b; b = c; c = t; }
  if (a > b) { const uint16_t t = a; a = b; b = t; }
  return b;
}
uint32_t slewPeriod(uint32_t previous, uint32_t requested) {
  const uint32_t limit = max(10UL, previous / PERIOD_SLEW_DIVISOR);
  return constrain(requested, max(MIN_PERIOD_US, previous - limit),
                   min(MAX_MEASURED_US, previous + limit));
}
void resetSpeedGuard() {
  speedCycleSumUs = 0; speedCycleSamples = 0;
  speedFastCycles = 0; speedCycleAverageUs = 0;
}
bool speedCycleTooFast(uint32_t interval) {
  // Called only for qualified, sequential CLOSED_LOOP intervals.
  // Six consecutive intervals cover all six sectors in either direction.
  speedCycleSumUs += interval;
  if (++speedCycleSamples < 6) return false;
  speedCycleAverageUs = speedCycleSumUs / 6UL;
  if (speedCycleSumUs < MIN_PERIOD_US * 6UL) {
    if (speedFastCycles < SPEED_GUARD_CYCLES) ++speedFastCycles;
  } else speedFastCycles = 0;
  speedCycleSumUs = 0; speedCycleSamples = 0;
  return speedFastCycles >= SPEED_GUARD_CYCLES;
}
void acceptCrossing(uint32_t crossUs) {
  const uint32_t expected = state == State::CLOSED_LOOP ? measuredPeriodUs : forcedPeriodUs;
  const uint32_t intoStep = crossUs - lastCommutationUs;
  // Exclude demagnetization spikes and late events near the next forced step.
  if (intoStep < expected / 5UL || intoStep > expected * 4UL / 5UL) {
    sectorRejectReason = intoStep < expected / 5UL ? 1 : 2;
    if (state == State::STARTUP && intoStep > expected * 4UL / 5UL && intoStep <= expected)
      sectorLateCandidate = true;
    ++rejectedCrossings; afterCount = 0; candidateValid = false; return;
  }
  const bool hadPrevious = previousCrossValid;
  uint32_t interval = expected;
  bool timingCoherent = true;
  if (previousCrossValid) {
    interval = crossUs - previousCrossUs;
    const bool sequential = activeStep == followingStep(previousCrossStep);
    sectorEdgeIntervalUs = interval;
    sectorEdgeExpectedUs = expected;
    sectorEdgeSequential = sequential;
    if (!sequential || interval < expected * 3UL / 5UL ||
        interval > expected * 7UL / 5UL ||
        interval < MIN_PERIOD_US / 2UL || interval > MAX_MEASURED_US) {
      sectorRejectReason = 3;
      ++rejectedCrossings;
      if (state == State::CLOSED_LOOP) {
        // An implausible candidate may be switching noise. Keep the previous
        // good timing and deadline, and wait for a valid crossing in this
        // same sector. The deadline still stops if no valid edge arrives.
        afterCount = 0; candidateValid = false;
        return;
      }
      lockCount = 0;
      timingCoherent = false;
      interval = expected;
    }
  }
  const uint32_t filteredPeriod = previousCrossValid
      ? (static_cast<uint32_t>(measuredPeriodUs) + interval) / 2UL : interval;
  const uint32_t nextPeriod = state == State::CLOSED_LOOP
      ? slewPeriod(measuredPeriodUs, filteredPeriod) : filteredPeriod;
  if (state == State::CLOSED_LOOP) {
    // Capture the triggering edge BEFORE either safety guard can stop outputs.
    // This prevents speed-limit faults from displaying zero timing diagnostics.
    const uint32_t confirmedAt = motorMicros();
    sectorConfirmDelayUs = confirmedAt - crossUs;
    sectorCommLeadUs = static_cast<int32_t>(crossUs + nextPeriod / 2UL - confirmedAt);
    minimumCommLeadUs = min(static_cast<int32_t>(minimumCommLeadUs), sectorCommLeadUs);
  }
  if (state == State::CLOSED_LOOP) {
    // One short sector must not stand in for motor speed. Average one full
    // electrical cycle, then require two consecutive fast cycles. During
    // confirmation, slewPeriod still floors commutation at 600 us.
    // Grossly fast estimates are stopped immediately, regardless of averaging.
    const bool cycleTooFast = hadPrevious && speedCycleTooFast(interval);
    if (filteredPeriod < HARD_FAST_PERIOD_US || cycleTooFast) {
      stopNow(Fault::SPEED_LIMIT);
      return;
    }
  }
  if (state == State::CLOSED_LOOP && sectorCommLeadUs < static_cast<int32_t>(MIN_COMM_LEAD_US)) {
    ++lateConfirmedCrossings;
    stopNow(Fault::COMM_LATE);
    return;
  }
  measuredPeriodUs = nextPeriod;
  previousCrossUs = crossUs;
  previousCrossStep = activeStep;
  previousCrossValid = true;
  crossedThisStep = true;
  ++crossingCount;
  crossingPosition[activeStep] = static_cast<uint16_t>(intoStep * 1000UL / expected);
  if (crossingsByStep[activeStep] != UINT16_MAX) ++crossingsByStep[activeStep];
  if (state == State::STARTUP) {
    // Count sequential, stable crossings across the full electrical cycle.
    // Centering is checked at handover below, not separately in every sector.
    const bool centered = intoStep >= expected * 2UL / 5UL &&
                          intoStep <= expected * 3UL / 5UL;
    const bool stableInterval = interval >= expected * 4UL / 5UL &&
                                interval <= expected * 6UL / 5UL;
    if (centered && stableInterval && timingCoherent) {
      if (centeredByStep[activeStep] != UINT16_MAX) ++centeredByStep[activeStep];
    }
    if (stableInterval && timingCoherent) {
      if (lockCount < LOCK_CROSSES) ++lockCount;
    } else lockCount = 0;
    if (lockCount > peakLockCount) peakLockCount = lockCount;
    // Smooth the transition without jumping commutation phase.
    const uint32_t oldDeadline = lastCommutationUs + expected;
    const uint32_t idealDeadline = crossUs + measuredPeriodUs / 2UL;
    const int32_t phaseOffset = static_cast<int32_t>(idealDeadline - oldDeadline);
    const int32_t correction = constrain(phaseOffset / 2,
        -static_cast<int32_t>(expected / 10UL), static_cast<int32_t>(expected / 10UL));
    if (hadPrevious && timingCoherent) {
      forcedPeriodUs = slewPeriod(expected, measuredPeriodUs);
    }
    scheduleAt(oldDeadline + correction);
    const bool phaseNearTimer = phaseOffset >= -static_cast<int32_t>(expected / 10UL) &&
                                phaseOffset <= static_cast<int32_t>(expected / 10UL);
    if (lockCount >= LOCK_CROSSES && phaseNearTimer) {
      // Keep the already bounded handover deadline. Seed the closed-loop
      // period from the forced estimate, not a one-step raw speed jump.
      measuredPeriodUs = forcedPeriodUs;
      resetSpeedGuard();
      state = State::CLOSED_LOOP;
      lockNotice = true;
    }
    return;
  }
  if (state == State::CLOSED_LOOP) {
    // 60 degree ZC-to-ZC interval -> wait 30 degrees from the actual ZC.
    scheduleAt(crossUs + measuredPeriodUs / 2UL);
  }
}
void processPhaseSample(uint32_t now, uint16_t u, uint16_t v, uint16_t w) {
  if (!motorActive(state)) return;
  // RUN only: warm median history with ON samples of this NEW sector.
  // resetSectorDetector clears history on every commutation. Startup keeps
  // its original discard policy, so alignment/acquisition is unchanged.
  if (state != State::CLOSED_LOOP && !due(now, blankUntilUs)) return;
  adcHistory[0][historySlot] = u;
  adcHistory[1][historySlot] = v;
  adcHistory[2][historySlot] = w;
  adcHistoryTime[historySlot] = now;
  historySlot = (historySlot + 1) % 3;
  if (historyCount < 3) ++historyCount;
  if (historyCount < 3) return;
  // Median3 represents approximately the middle acquisition, including
  // pulse-skipping gaps. Use that time instead of assuming 50 us delay.
  now = adcHistoryTime[(historySlot + 1) % 3];
  // Never update rails, polarity evidence or crossing candidates while the
  // median's middle timestamp is still inside demagnetization blanking.
  // The first eligible window may include ONE earlier sample; median3 can
  // reject one transient, but this policy still needs scope validation.
  if (state == State::CLOSED_LOOP && !due(now, blankUntilUs)) return;
  const uint16_t raw[3] = {
    median3(adcHistory[0][0], adcHistory[0][1], adcHistory[0][2]),
    median3(adcHistory[1][0], adcHistory[1][1], adcHistory[1][2]),
    median3(adcHistory[2][0], adcHistory[2][1], adcHistory[2][2])
  };
  const Step &step = STEPS[activeStep];
  const uint16_t high = raw[step.high], low = raw[step.low], floating = raw[step.floating];
  const uint16_t span = high > low ? high - low : 0;
  for (uint8_t i = 0; i < 3; ++i) latestRaw[i] = raw[i];
  latestFloatingIndex = step.floating;
  // All diagnostic values come from the same filtered sample.
  latestNeutral = (static_cast<uint32_t>(raw[0]) + raw[1] + raw[2]) / 3UL;
  latestError = phaseError(high, low, floating);
  // During PWM ON, the selected LOW terminal must be near ground. Merely
  // checking HIGH-LOW span can accept floating/mistimed rail samples as BEMF.
  const uint16_t lowLimit = max(LOW_CLAMP_FLOOR_COUNTS,
                               static_cast<uint16_t>(high / LOW_CLAMP_DIVISOR));
  const bool lowClamped = low <= lowLimit;
  latestSignalValid = span >= MIN_DRIVEN_SPAN && high < 4000 && lowClamped;
  if (!latestSignalValid) {
    ++invalidPhaseSamples;
    if (!lowClamped) {
      ++lowClampRejected;
      if (lowRejectedByStep[activeStep] != UINT16_MAX) ++lowRejectedByStep[activeStep];
      // Discard this rail sample and any pending edge. A single bad pulse
      // may preserve the qualified before-side, but no edge may interpolate
      // across it; three consecutive bad samples reset the detector below.
      previousErrorValid = false;
      candidateValid = false;
    }
    ++sectorInvalidSamples;
    if (++badSignalCount >= SIGNAL_BAD_LIMIT) {
      stopNow(high >= 4000 ? Fault::ADC_SATURATED : Fault::PHASE_SIGNAL);
    }
    // Drop the invalid voltage. Preserve an already-qualified pre-crossing
    // side briefly; a single bad ON sample must not erase an entire sector.
    if (consecutiveInvalidSamples < 255) ++consecutiveInvalidSamples;
    afterCount = 0;
    if (consecutiveInvalidSamples >= EDGE_CONFIRMATIONS) {
      seenBefore = false; previousErrorValid = false;
      beforeCount = 0; candidateValid = false;
    }
    return;
  }
  consecutiveInvalidSamples = 0;
  badSignalCount = 0;
  ++sectorValidSamples;
  if (sectorValidSamples == 1) sectorFirstValidIntoUs = now - lastCommutationUs;
  if (!bemfSectorEligible()) return;
  const bool rising = forwardDirection ? step.rising : !step.rising;
  const int32_t signedError = rising ? latestError : -latestError;
  sectorErrorMin = min(sectorErrorMin, signedError);
  sectorErrorMax = max(sectorErrorMax, signedError);
  if (crossedThisStep) return;
  const int32_t hysteresis = 3 * max(8UL, static_cast<uint32_t>(span) / 128UL);
  if (signedError < -hysteresis) ++sectorNegativeSamples;
  const uint32_t sectorPeriod = state == State::CLOSED_LOOP ? measuredPeriodUs : forcedPeriodUs;
  // Fractional duty skips a PWM period even near full pulse density.
  // Allow two carrier periods plus timestamp margin instead of losing
  // the qualified polarity when the sector becomes shorter than 1.6 ms.
  const uint32_t maxGap = max(PWM_SAMPLE_GAP_US,
                             max(MIN_CROSS_GAP_US, sectorPeriod / 8UL));
  // Any interpolation below is between valid voltages only. A long gap
  // cannot establish a zero-crossing, even if the previous polarity fits.
  if (previousErrorValid && now - previousErrorUs > maxGap) {
    seenBefore = false; beforeCount = 0;
    candidateValid = false; previousErrorValid = false;
  }
  if (!seenBefore) {
    // At high speed the pre-crossing interval may contain fewer ON samples.
    // Median3 remains active; require two before-side confirmations for RUN,
    // after-side qualification below remains at least two filtered samples.
    const uint8_t requiredBefore = state == State::CLOSED_LOOP && sectorPeriod < FAST_SECTOR_US
        ? 2 : EDGE_CONFIRMATIONS;
    if (signedError < -hysteresis) {
      if (beforeCount + 1 > sectorPeakBeforeCount) sectorPeakBeforeCount = beforeCount + 1;
      if (++beforeCount >= requiredBefore) seenBefore = true;
    } else if (signedError > hysteresis) beforeCount = 0;
  }
  // Interpolate the first negative-to-positive sign change using the
  // filtered sample timestamp, including any skipped PWM periods.
  if (seenBefore && previousErrorValid && previousError <= 0 && signedError > 0) {
    const uint32_t dt = now - previousErrorUs;
    const int32_t difference = signedError - previousError;
    const uint32_t offset = difference > 0
        ? static_cast<uint32_t>((static_cast<int64_t>(-previousError) * dt) / difference) : 0;
    candidateUs = previousErrorUs + offset;
    candidateValid = true;
  }
  if (seenBefore && candidateValid && signedError > hysteresis) {
    // At <1 ms/sector, a third after-side sample spends another 66.7 us
    // of the 30-degree commutation budget. Median3 + two before + two after
    // confirmations remain required; startup retains all three after samples.
    const uint8_t requiredAfter = state == State::CLOSED_LOOP && sectorPeriod < FAST_AFTER_SECTOR_US
        ? 2 : EDGE_CONFIRMATIONS;
    if (++afterCount >= requiredAfter) acceptCrossing(candidateUs);
  } else if (signedError < -hysteresis) {
    afterCount = 0;
    candidateValid = false;
  }
  previousError = signedError;
  previousErrorUs = now;
  previousErrorValid = true;
}
bool adcPulseIsOn() {
  // ADC and PWM update IRQs have equal priority. If UIF is still pending,
  // hardware has loaded the prepared pulse but its ISR has not tagged it yet.
  const bool pulse = (TIM1->SR & TIM_SR_UIF) ? pwmPreparedPulse : pwmPulseEnabled;
  return pwmDriveEnabled && pulse && (TIM1->BDTR & TIM_BDTR_MOE);
}
void recordScopeSample(uint32_t now, uint16_t u, uint16_t v, uint16_t w) {
  if (!scopeCapturing) return;
  if (!motorActive(state) || now - scopeStartedUs >= SCOPE_MAX_CAPTURE_US) {
    scopeCapturing = false; scopeReady = scopeCount > 0; return;
  }
  // Keep every second PWM-ON conversion. Actual t_us accompanies each sample,
  // so pulse-skipped low duty cannot masquerade as uniform-rate acquisition.
  if (++scopeDecimation < 2) return;
  scopeDecimation = 0;
  const uint16_t index = scopeCount;
  const uint16_t limit = scopeLive ? SCOPE_LIVE_SAMPLES : SCOPE_MAX_SAMPLES;
  if (index >= limit) { scopeCapturing = false; scopeReady = true; return; }
  ScopeSample &sample = scopeSamples[index];
  sample.tUs = now; sample.u = u; sample.v = v; sample.w = w;
  sample.floating = STEPS[activeStep].floating;
  sample.step = activeStep + 1;
  scopeCount = index + 1;
  if (scopeCount == limit) { scopeCapturing = false; scopeReady = true; }
}
extern "C" void ADC_IRQHandler(void) {
  if (!(ADC3->SR & ADC_SR_JEOC)) return;
  const bool complete = (ADC1->SR & ADC_SR_JEOC) && (ADC2->SR & ADC_SR_JEOC);
  const uint16_t u = ADC1->JDR1, v = ADC2->JDR1, w = ADC3->JDR1;
  ADC1->SR = 0; ADC2->SR = 0; ADC3->SR = 0;
  if (!complete) { ++skippedAdcSets; return; }
  ++adcSamples;
  const uint32_t now = motorMicros();
  lastAdcUs = now;
  // ADC trigger remains alive in skipped periods, but those voltages are
  // PWM-OFF data and MUST NOT enter the ON-time zero-cross detector.
  if (!adcPulseIsOn()) return;
  recordScopeSample(now, u, v, w);
  processPhaseSample(now, u, v, w);
}


/*
 * Current monitor: PC0/PC1/PC2 -> assumed U/V/W, ADC1 IN10/11/12.
 * Verify phase mapping on the actual board before interpreting these labels.
 * Regular scan + DMA2 Stream0 Channel0 is separate from BEMF injected JDRs.
 * STOP: asynchronous three-channel calibration. RUN: single selected LOW,
 * hardware TIM3_TRGO (OC1REF PWM2) at 2 us after TIM1 update, DMA completion qualified
 * inside PWM ON. TIM3 is reserved; timing/source settling require scope verification.
 * No amperes are claimed until shunt, INA gain, VDDA and polarity are verified.
 * NO current-trip protection here; external current limiting remains necessary.
 */
constexpr uint32_t CURRENT_SAMPLE_PERIOD_US = 10000; // 100 scans/s
// Diagnostic conversion only: INA181A1, 1 mOhm, direct OUT-to-ADC connection.
// VDDA is nominal, NOT measured. Verify wiring, VDDA and polarity on hardware.
constexpr uint32_t CURRENT_VDDA_MV = 3300;
constexpr uint32_t CURRENT_ADC_FULL_SCALE = 4095;
constexpr uint32_t CURRENT_GAIN = 20;
constexpr uint32_t CURRENT_SHUNT_MILLIOHM = 1;
static_assert(CURRENT_GAIN > 0 && CURRENT_SHUNT_MILLIOHM > 0, "Invalid current scale");

int32_t currentDeltaMilliAmps(int32_t delta) {
  const int64_t denominator = static_cast<int64_t>(CURRENT_ADC_FULL_SCALE) * CURRENT_GAIN * CURRENT_SHUNT_MILLIOHM;
  const int64_t numerator = static_cast<int64_t>(delta) * CURRENT_VDDA_MV * 1000;
  // Symmetric rounding of positive and negative values; no unsigned wrap.
  return static_cast<int32_t>((numerator >= 0 ? numerator + denominator / 2 : numerator - denominator / 2) / denominator);
}
constexpr uint32_t CURRENT_DMA_TIMEOUT_US = 2000;
constexpr uint16_t CURRENT_ZERO_SAMPLES = 64;
constexpr uint16_t CURRENT_ZERO_TRIM = 8; // remove lowest/highest 8 of 64 per channel
constexpr uint16_t CURRENT_ZERO_MAX_SPREAD = 48; // central 48-sample span; diagnostic threshold, not amp limit
alignas(4) volatile uint16_t currentDmaRaw[3] = {};
uint16_t currentRaw[3] = {}, currentZero[3] = {};
uint32_t currentScans = 0, currentFailures = 0;
uint32_t currentScanAtUs = 0, currentLastScanUs = 0;
volatile bool currentBusy = false, currentMonitorOK = false;
bool currentHasSample = false;
volatile bool currentScanEnabled = false; // boot with no regular conversions: BEMF-first baseline
volatile bool currentRegularControlsActive = false;
volatile bool currentPwmRequest = false, currentPwmBusy = false, currentPwmReady = false;
volatile uint32_t currentPwmStartedUs = 0, currentPwmEpoch = 0, currentPwmSectorUs = 0;
volatile uint32_t currentPwmStartTicks = 0;
volatile uint32_t currentPwmAccepted = 0, currentPwmDropped = 0, currentWindowMisses = 0;
volatile uint8_t currentPwmChannel = 0, currentPwmStep = 0;
volatile uint16_t currentPwmRaw = 0;
volatile uint32_t currentPwmCapturedUs = 0;
uint32_t currentTicksPerUs = 0;
constexpr uint32_t CURRENT_PWM_PERIOD_US = 1000; // diagnostic request rate, not protection
constexpr uint32_t CURRENT_PWM_OFFSET_US = 2; // timer trigger after ON leading edge
constexpr uint32_t CURRENT_PWM_COMPLETE_US = 8; // conservative latest DMA IRQ in pulse
constexpr uint32_t CURRENT_PWM_ARM_MARGIN_US = 8; // do not arm close to rollover
uint32_t currentCarrierUs = 0;
bool currentTriggerTimerOK = false;
volatile uint32_t currentLastArmTicks = 0, currentLastDoneTicks = 0, currentLastOnTicks = 0;
uint8_t currentSelectedChannel = 0, currentSelectedStep = 0;
uint16_t currentSelectedRaw = 0;
uint32_t currentSelectedUs = 0;
bool currentSelectedReady = false;
bool currentZeroReady = false, currentCalibrating = false;
// Stopped DC calibration helper; main-loop only, never changes motor timing.
constexpr uint16_t CURRENT_TEST_SAMPLES = 64;
bool currentTestActive = false, currentTestReady = false, currentTestSkip = false;
uint16_t currentTestCount = 0, currentTestZero[3] = {};
uint16_t currentTestMin[3] = {}, currentTestMax[3] = {};
uint32_t currentTestSum[3] = {}, currentTestStartedUs = 0;

int32_t currentTestMilliAmps(uint8_t channel) {
  if (!currentTestCount || channel >= 3) return 0;
  const int64_t deltaSum = static_cast<int64_t>(currentTestSum[channel]) -
      static_cast<int64_t>(currentTestZero[channel]) * currentTestCount;
  const int64_t numerator = deltaSum * CURRENT_VDDA_MV * 1000;
  const int64_t denominator = static_cast<int64_t>(currentTestCount) *
      CURRENT_ADC_FULL_SCALE * CURRENT_GAIN * CURRENT_SHUNT_MILLIOHM;
  return static_cast<int32_t>((numerator >= 0 ? numerator + denominator / 2 :
      numerator - denominator / 2) / denominator);
}
void printCurrentTest() {
  if (!currentTestReady) { Serial.println(F("No completed CTEST; STOP, CZERO at zero current, then CTEST.")); return; }
  char line[210];
  Serial.println(F("CTEST DC stopped: arithmetic mean, not PWM/RMS/peak. Scale nominal; protection OFF."));
  for (uint8_t i = 0; i < 3; ++i) {
    const int32_t deltaSum = static_cast<int32_t>(currentTestSum[i]) -
        static_cast<int32_t>(currentTestZero[i]) * currentTestCount;
    snprintf(line, sizeof(line), "channel=%c samples=%u zero=%u raw_min/max=%u/%u spread=%u delta_sum=%ld mean_mA=%ld rail=%u\n",
        "UVW"[i], currentTestCount, currentTestZero[i], currentTestMin[i], currentTestMax[i],
        currentTestMax[i] - currentTestMin[i], static_cast<long>(deltaSum),
        static_cast<long>(currentTestMilliAmps(i)), currentTestMin[i] < 64 || currentTestMax[i] > 4031);
    Serial.print(line);
  }
}
void beginCurrentTest() {
  if (motorActive(state) || currentCalibrating || currentTestActive ||
      !currentScanEnabled || !currentMonitorOK || !currentZeroReady) {
    Serial.println(F("Rejected: CTEST requires stopped motor, CSCAN ON, valid CZERO, no other collection.")); return;
  }
  currentTestReady = false; currentTestActive = true; currentTestCount = 0;
  currentTestSkip = currentBusy; currentTestStartedUs = motorMicros();
  for (uint8_t i = 0; i < 3; ++i) {
    currentTestZero[i] = currentZero[i]; currentTestSum[i] = 0;
    currentTestMin[i] = 4095; currentTestMax[i] = 0;
  }
  Serial.println(F("CTEST: hold known DC current steady for 64 NEW stopped scans (~0.64 s). No gates driven."));
}
void consumeCurrentTest() {
  if (!currentTestActive) return;
  if (currentTestSkip) { currentTestSkip = false; return; }
  for (uint8_t i = 0; i < 3; ++i) {
    currentTestSum[i] += currentRaw[i];
    currentTestMin[i] = min(currentTestMin[i], currentRaw[i]);
    currentTestMax[i] = max(currentTestMax[i], currentRaw[i]);
  }
  if (++currentTestCount == CURRENT_TEST_SAMPLES) {
    currentTestActive = false; currentTestReady = true; printCurrentTest();
  }
}
void compareCurrentReference(uint8_t channel, int32_t referenceMa) {
  if (motorActive(state) || currentTestActive || !currentTestReady || channel >= 3 ||
      referenceMa == 0 || referenceMa < -5000 || referenceMa > 5000) {
    Serial.println(F("Rejected: completed STOP CTEST required; CREF U|V|W signed_mA (-5000..5000, nonzero).")); return;
  }
  if (currentTestMin[channel] < 64 || currentTestMax[channel] > 4031) {
    Serial.println(F("Rejected: near-rail CTEST data; check sensor before comparison.")); return;
  }
  const int32_t measuredMa = currentTestMilliAmps(channel);
  char line[210];
  snprintf(line, sizeof(line), "CREF channel=%c reference_mA=%ld estimated_mean_mA=%ld error_mA=%ld relative_error_permille=%ld polarity_match=%u\n",
      "UVW"[channel], static_cast<long>(referenceMa), static_cast<long>(measuredMa),
      static_cast<long>(measuredMa - referenceMa),
      static_cast<long>((static_cast<int64_t>(measuredMa) - referenceMa) * 1000 / referenceMa),
      measuredMa != 0 && (measuredMa > 0) == (referenceMa > 0));
  Serial.print(line);
  Serial.println(F("Comparison only: scale/zero NOT changed. Use signed current through THIS shunt, not supply current during RUN."));
}
volatile uint32_t currentDmaIrqs = 0;
// Plain value snapshots: copied under critical sections, no live DR reads.
struct CurrentHwSnapshot {
  volatile uint32_t reason, atUs, elapsedUs, step, epoch, expectedEpoch;
  volatile uint32_t t1cr2, t1cnt, t1sr, t3cr1, t3cr2, t3smcr, t3cnt, t3sr, t3ccr1;
  volatile uint32_t acr1, acr2, asr, sqr1, sqr3, dcr, ndtr, lisr, irqEnabled, irqPending;
};
// Explicit prototypes prevent Arduino's auto-prototypes from preceding this type.
void readCurrentHardwareSnapshot(CurrentHwSnapshot &s, uint32_t reason);
void printCurrentHardwareSnapshot(const CurrentHwSnapshot &s, const char *label);
CurrentHwSnapshot currentFailureSnapshot = {};
volatile bool currentFailureSnapshotReady = false;
uint32_t currentFailureReported = 0;
void readCurrentHardwareSnapshot(CurrentHwSnapshot &s, uint32_t reason) {
  // Caller owns critical section. Register reads do not consume ADC/DMA data.
  s.reason = reason; s.atUs = motorMicros();
  s.elapsedUs = s.atUs - currentPwmStartedUs; s.step = activeStep + 1;
  s.epoch = currentPulseEpoch; s.expectedEpoch = currentPwmEpoch;
  s.t1cr2 = TIM1->CR2; s.t1cnt = TIM1->CNT; s.t1sr = TIM1->SR;
  s.t3cr1 = TIM3->CR1; s.t3cr2 = TIM3->CR2; s.t3smcr = TIM3->SMCR;
  s.t3cnt = TIM3->CNT; s.t3sr = TIM3->SR; s.t3ccr1 = TIM3->CCR1;
  s.acr1 = ADC1->CR1; s.acr2 = ADC1->CR2; s.asr = ADC1->SR;
  s.sqr1 = ADC1->SQR1; s.sqr3 = ADC1->SQR3;
  s.dcr = DMA2_Stream0->CR; s.ndtr = DMA2_Stream0->NDTR; s.lisr = DMA2->LISR;
  s.irqEnabled = NVIC_GetEnableIRQ(DMA2_Stream0_IRQn);
  s.irqPending = NVIC_GetPendingIRQ(DMA2_Stream0_IRQn);
}
void saveCurrentFailureSnapshot(uint32_t reason) {
  // Preserve the FIRST failure until reset, before cleanup destroys evidence.
  const uint32_t key = enterCritical();
  if (!currentFailureSnapshotReady) {
    readCurrentHardwareSnapshot(currentFailureSnapshot, reason);
    currentFailureSnapshotReady = true;
  }
  leaveCritical(key);
}
void printCurrentHardwareSnapshot(const CurrentHwSnapshot &s, const char *label) {
  char line[240];
  snprintf(line, sizeof(line), "%s reason=%lu at_us=%lu elapsed_us=%lu step=%lu epoch/expected=%lu/%lu\n",
           label, static_cast<unsigned long>(s.reason), static_cast<unsigned long>(s.atUs),
           static_cast<unsigned long>(s.elapsedUs), static_cast<unsigned long>(s.step),
           static_cast<unsigned long>(s.epoch), static_cast<unsigned long>(s.expectedEpoch));
  Serial.print(line);
  snprintf(line, sizeof(line), "TIM1 CR2=0x%08lx CNT=%lu SR=0x%08lx; TIM3 CR1=0x%08lx CR2=0x%08lx SMCR=0x%08lx CNT=%lu CCR1=%lu SR=0x%08lx\n",
           static_cast<unsigned long>(s.t1cr2), static_cast<unsigned long>(s.t1cnt), static_cast<unsigned long>(s.t1sr),
           static_cast<unsigned long>(s.t3cr1), static_cast<unsigned long>(s.t3cr2), static_cast<unsigned long>(s.t3smcr),
           static_cast<unsigned long>(s.t3cnt), static_cast<unsigned long>(s.t3ccr1), static_cast<unsigned long>(s.t3sr));
  Serial.print(line);
  snprintf(line, sizeof(line), "ADC1 CR1=0x%08lx CR2=0x%08lx SR=0x%08lx SQR1=0x%08lx SQR3=0x%08lx\n",
           static_cast<unsigned long>(s.acr1), static_cast<unsigned long>(s.acr2), static_cast<unsigned long>(s.asr),
           static_cast<unsigned long>(s.sqr1), static_cast<unsigned long>(s.sqr3));
  Serial.print(line);
  snprintf(line, sizeof(line), "DMA2S0 CR=0x%08lx NDTR=%lu LISR=0x%08lx NVIC enabled/pending=%lu/%lu\n",
           static_cast<unsigned long>(s.dcr), static_cast<unsigned long>(s.ndtr), static_cast<unsigned long>(s.lisr),
           static_cast<unsigned long>(s.irqEnabled), static_cast<unsigned long>(s.irqPending));
  Serial.print(line);
}
void printCurrentHardwareDiagnosis() {
  CurrentHwSnapshot live = {}, failed = {};
  const uint32_t key = enterCritical();
  readCurrentHardwareSnapshot(live, 0);
  const bool hasFailure = currentFailureSnapshotReady;
  if (hasFailure) failed = currentFailureSnapshot;
  leaveCritical(key);
  Serial.println(F("CDIAG reason: 0=live 1=PWM DMA timeout 2=STOP DMA error 3=STOP DMA timeout 4=PWM DMA IRQ error."));
  if (hasFailure) printCurrentHardwareSnapshot(failed, "SAVED before cleanup");
  else Serial.println(F("No saved current failure."));
  printCurrentHardwareSnapshot(live, "LIVE (not simultaneous with saved failure)");
  Serial.println(F("TIM3 CC1IF/TIF cleared at arm; flags show events since arm. ADC SR may be cleared by BEMF ISR; absence is not proof of no conversion."));
}
bool currentZeroSkipPending = false;
uint16_t currentZeroCount = 0;
uint16_t currentZeroMin[3] = {}, currentZeroMax[3] = {};
uint16_t currentZeroHistory[3][CURRENT_ZERO_SAMPLES] = {};
uint16_t currentZeroTrimMin[3] = {}, currentZeroTrimMax[3] = {};
bool currentZeroStatsReady = false;
constexpr uint32_t CURRENT_DMA_CLEAR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 |
    DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;

void configureCurrentMonitor() {
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIOC->MODER |= 0x3FUL; GPIOC->PUPDR &= ~0x3FUL;
  __HAL_RCC_DMA2_CLK_ENABLE();
  // Do not commandeer a DMA stream that another component already owns.
  if (DMA2_Stream0->CR & DMA_SxCR_EN) return;
  ADC1->SMPR1 = (ADC1->SMPR1 & ~0x1FFUL) | (3UL << 0) | (3UL << 3) | (3UL << 6); // 56 cycles
  ADC1->SQR1 = 2UL << ADC_SQR1_L_Pos; // three regular conversions
  ADC1->SQR2 = 0;
  ADC1->SQR3 = 10UL | (11UL << 5) | (12UL << 10);
  // SCAN/DMA are enabled only when requested, after any old scan finishes.
  DMA2_Stream0->CR = DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
  DMA2_Stream0->FCR = 0; // direct mode, no interrupt
  DMA2_Stream0->PAR = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ADC1->DR));
  DMA2_Stream0->M0AR = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(currentDmaRaw));
  DMA2->LIFCR = CURRENT_DMA_CLEAR;
  const uint32_t timHz = HAL_RCC_GetPCLK2Freq() * ((RCC->CFGR & RCC_CFGR_PPRE2) ? 2UL : 1UL);
  currentTicksPerUs = timHz / ((TIM1->PSC + 1UL) * 1000000UL);
  currentCarrierUs = currentTicksPerUs ? (TIM1->ARR + 1UL) / currentTicksPerUs : 0;
  // RM0090: TIM3 ITR0=TIM1 TRGO; ADC regular EXTSEL=TIM3_TRGO (1000).
  // TIM3 is internal only (no GPIO, no interrupt). Refuse to take an active timer.
  __HAL_RCC_TIM3_CLK_ENABLE();
  if (!(TIM3->CR1 & TIM_CR1_CEN) && !TIM3->DIER && currentTicksPerUs) {
    const uint32_t t3Hz = HAL_RCC_GetPCLK1Freq() * ((RCC->CFGR & RCC_CFGR_PPRE1) ? 2UL : 1UL);
    const uint32_t offsetTicks = (t3Hz / 1000000UL) * CURRENT_PWM_OFFSET_US;
    if (offsetTicks && offsetTicks < 65535UL) {
      TIM3->CR1 = 0; TIM3->CR2 = 0; TIM3->SMCR = 0; TIM3->DIER = 0;
      TIM3->PSC = 0; TIM3->ARR = 65535; TIM3->CCR1 = offsetTicks;
      // PWM2 OC1REF is LOW after reset, rises at CCR1, returns LOW next reset.
      // Unlike frozen compare, this supplies an explicit edge to the TRGO mux.
      TIM3->CCMR1 = TIM_OCMODE_PWM2; TIM3->CCER = TIM_CCER_CC1E;
      TIM3->CR2 = TIM_TRGO_OC1REF; // internal only: no alternate-function GPIO
      TIM3->EGR = TIM_EGR_UG; TIM3->SR = 0;
      TIM3->SMCR = TIM_SMCR_SMS_2; // reset mode, TS=ITR0
      TIM1->CR2 = (TIM1->CR2 & ~TIM_CR2_MMS) | TIM_CR2_MMS_1; // TRGO=update
      TIM3->CR1 = TIM_CR1_CEN;
      currentTriggerTimerOK = true;
    }
  }
  NVIC_SetPriority(DMA2_Stream0_IRQn, 3); // below motor IRQs, above USB
  NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  currentMonitorOK = true;
}
void tryStartPwmCurrent() {
  // MAIN LOOP, called in a short critical section. Arm for the NEXT carrier.
  // No SWSTART in RUN: hardware TIM3_TRGO starts at a fixed timer position.
  if (!currentScanEnabled || !currentMonitorOK || !currentPwmRequest || currentBusy ||
      currentPwmBusy || currentPwmReady || !currentTriggerTimerOK || state != State::CLOSED_LOOP) return;
  const uint32_t now = motorMicros();
  if (!due(now, blankUntilUs) || !pwmPreparedPulse || (TIM1->SR & TIM_SR_UIF) ||
      !pwmDriveEnabled || !(TIM1->BDTR & TIM_BDTR_MOE)) return;
  const uint32_t count = TIM1->CNT;
  const uint32_t periodTicks = TIM1->ARR + 1UL;
  // Arm AFTER this cycle's compare and comfortably BEFORE the next rollover.
  // Sampling can occur in the next cycle even if we arm while this cycle is OFF.
  if (!currentTicksPerUs || count <= CURRENT_PWM_COMPLETE_US * currentTicksPerUs ||
      count + CURRENT_PWM_ARM_MARGIN_US * currentTicksPerUs >= periodTicks ||
      currentPreparedOnTicks <= (CURRENT_PWM_OFFSET_US + 3UL) * currentTicksPerUs) {
    ++currentWindowMisses; return;
  }
  if (DMA2_Stream0->CR & DMA_SxCR_EN) return;
  // Re-arm the ADC DMA request generator as well as the stream. DDS=0 makes
  // this a single DMA transfer, even if the main loop/IRQ is delayed. A fresh
  // DMA 0->1 transition is needed for the next one-shot. Never power-cycle ADC1
  // or change its injected trigger, sample channels or JDR registers here.
  ADC1->CR2 &= ~(ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS);
  ADC1->SR = ~(ADC_SR_OVR | ADC_SR_EOC | ADC_SR_STRT); // W0C, preserve JEOC/JSTRT
  currentPwmChannel = STEPS[activeStep].low;
  currentPwmStep = activeStep; currentPwmSectorUs = lastCommutationUs;
  currentPwmEpoch = currentPulseEpoch + 1UL; currentPwmStartedUs = now;
  currentPwmStartTicks = count;
  currentLastArmTicks = count; currentLastOnTicks = currentPreparedOnTicks;
  currentPwmBusy = true; currentPwmRequest = false;
  TIM3->SR = ~(TIM_SR_CC1IF | TIM_SR_TIF); // W0C: identify timer events AFTER this arm
  // One selected low-side conversion, 15-cycle acquisition. Phase SMPR2/JSQR untouched.
  ADC1->SMPR1 = (ADC1->SMPR1 & ~0x1FFUL) | (1UL << 0) | (1UL << 3) | (1UL << 6);
  ADC1->SQR1 = 0; ADC1->SQR3 = 10UL + currentPwmChannel;
  ADC1->CR1 &= ~ADC_CR1_SCAN;
  DMA2->LIFCR = CURRENT_DMA_CLEAR; DMA2_Stream0->NDTR = 1;
  DMA2_Stream0->CR = DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
      DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
  __DMB(); DMA2_Stream0->CR |= DMA_SxCR_EN;
  ADC1->CR2 = (ADC1->CR2 & ~(ADC_CR2_EXTSEL | ADC_CR2_EXTEN)) |
      ADC_EXTERNALTRIGCONV_T3_TRGO | ADC_CR2_EXTEN_0 | ADC_CR2_DMA;
  currentRegularControlsActive = true;
}
extern "C" void DMA2_Stream0_IRQHandler(void) {
  ++currentDmaIrqs;
  const uint32_t flags = DMA2->LISR;
  if (!currentPwmBusy) { DMA2->LIFCR = CURRENT_DMA_CLEAR; return; }
  const uint32_t key = enterCritical(); // validation must not race sector/pulse changes
  const uint32_t now = motorMicros();
  const uint32_t completedTicks = TIM1->CNT;
  currentLastDoneTicks = completedTicks;
  if (flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) saveCurrentFailureSnapshot(4);
  ADC1->CR2 &= ~ADC_CR2_EXTEN; // one armed event; preserve injected JEXTEN
  const bool valid = (flags & DMA_LISR_TCIF0) &&
      !(flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) &&
      currentScanEnabled && state == State::CLOSED_LOOP && pwmDriveEnabled && pwmPulseEnabled &&
      (TIM1->BDTR & TIM_BDTR_MOE) && !(TIM1->SR & TIM_SR_UIF) &&
      activeStep == currentPwmStep && lastCommutationUs == currentPwmSectorUs &&
      currentPulseEpoch == currentPwmEpoch &&
      completedTicks >= CURRENT_PWM_OFFSET_US * currentTicksPerUs &&
      completedTicks <= CURRENT_PWM_COMPLETE_US * currentTicksPerUs &&
      completedTicks < currentOnTicks &&
      now - currentPwmStartedUs <= currentCarrierUs + CURRENT_PWM_COMPLETE_US;
  if (valid) {
    __DMB(); currentPwmRaw = currentDmaRaw[0]; currentPwmCapturedUs = now;
    ++currentPwmAccepted; currentPwmReady = true;
  } else ++currentPwmDropped;
  DMA2_Stream0->CR &= ~(DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
  DMA2->LIFCR = CURRENT_DMA_CLEAR;
  currentPwmBusy = false;
  leaveCritical(key);
}
void abortCurrentZero(const char *reason) {
  currentCalibrating = false;
  Serial.print(F("CURRENT ZERO aborted: ")); Serial.println(reason);
}
void printCurrentZeroStats() {
  if (!currentZeroStatsReady) { Serial.println(F("zero statistics unavailable; complete CZERO first.")); return; }
  char line[180];
  for (uint8_t i = 0; i < 3; ++i) {
    snprintf(line, sizeof(line), "zero channel=%c raw_min/max=%u/%u raw_spread=%u trimmed_min/max=%u/%u trimmed_spread=%u limit=%u counts\n",
             "UVW"[i], currentZeroMin[i], currentZeroMax[i], currentZeroMax[i] - currentZeroMin[i],
             currentZeroTrimMin[i], currentZeroTrimMax[i], currentZeroTrimMax[i] - currentZeroTrimMin[i],
             CURRENT_ZERO_MAX_SPREAD);
    Serial.print(line);
  }
}
void finishCurrentZero() {
  uint16_t proposed[3] = {};
  // Bounded insertion sort of 64 samples, MAIN LOOP only, IRQs enabled.
  // No new motor start is accepted while calibration is active.
  for (uint8_t channel = 0; channel < 3; ++channel) {
    uint16_t *values = currentZeroHistory[channel];
    for (uint16_t i = 1; i < CURRENT_ZERO_SAMPLES; ++i) {
      const uint16_t value = values[i]; uint16_t j = i;
      while (j && values[j - 1] > value) { values[j] = values[j - 1]; --j; }
      values[j] = value;
    }
    currentZeroTrimMin[channel] = values[CURRENT_ZERO_TRIM];
    currentZeroTrimMax[channel] = values[CURRENT_ZERO_SAMPLES - CURRENT_ZERO_TRIM - 1];
    uint32_t sum = 0;
    for (uint16_t i = CURRENT_ZERO_TRIM; i < CURRENT_ZERO_SAMPLES - CURRENT_ZERO_TRIM; ++i) sum += values[i];
    const uint16_t count = CURRENT_ZERO_SAMPLES - 2 * CURRENT_ZERO_TRIM;
    proposed[channel] = static_cast<uint16_t>((sum + count / 2) / count);
  }
  currentZeroStatsReady = true;
  printCurrentZeroStats();
  for (uint8_t i = 0; i < 3; ++i) {
    if (currentZeroMin[i] < 64 || currentZeroMax[i] > 4031) {
      abortCurrentZero("near-rail sample; see channel statistics"); return;
    }
    if (currentZeroTrimMax[i] - currentZeroTrimMin[i] > CURRENT_ZERO_MAX_SPREAD) {
      abortCurrentZero("persistent noise/drift in central samples; see statistics"); return;
    }
  }
  // Commit all three channels only after every check has passed.
  for (uint8_t i = 0; i < 3; ++i) currentZero[i] = proposed[i];
  currentZeroReady = true; currentCalibrating = false;
  Serial.println(F("CURRENT ZERO ready: trimmed mean of central 48/64 samples (RAM only). Send CURRENT."));
}
void consumeCurrentScan() {
  for (uint8_t i = 0; i < 3; ++i) currentRaw[i] = currentDmaRaw[i];
  currentHasSample = true; ++currentScans;
  consumeCurrentTest();
  if (!currentCalibrating) return;
  if (motorActive(state)) { abortCurrentZero("outputs active"); return; }
  if (currentZeroSkipPending) { currentZeroSkipPending = false; return; }
  for (uint8_t i = 0; i < 3; ++i) {
    currentZeroHistory[i][currentZeroCount] = currentRaw[i];
    currentZeroMin[i] = min(currentZeroMin[i], currentRaw[i]);
    currentZeroMax[i] = max(currentZeroMax[i], currentRaw[i]);
  }
  if (++currentZeroCount < CURRENT_ZERO_SAMPLES) return;
  finishCurrentZero();
}
void pollCurrentMonitor() {
  if (currentTestActive && (!currentMonitorOK || !currentScanEnabled || motorActive(state) ||
      motorMicros() - currentTestStartedUs > 2000000UL)) {
    currentTestActive = false; currentTestReady = false;
    Serial.println(F("CTEST aborted: monitor/state changed or 2 s collection timeout."));
  }
  if (currentFailures != currentFailureReported) {
    currentFailureReported = currentFailures;
    Serial.println(F("CURRENT MONITOR failed; current trip OFF. Send CDIAG before reset."));
  }
  if (!currentMonitorOK) {
    // With scanning requested, do not continue driving after monitor failure.
    // This is an acquisition-failure interlock, NOT an ampere-limit guarantee.
    const uint32_t key = enterCritical();
    if (currentScanEnabled && motorActive(state)) stopNow(Fault::CURRENT_MONITOR);
    leaveCritical(key);
    return;
  }
  const uint32_t now = motorMicros();
  const uint32_t pwmKey = enterCritical();
  if (currentPwmReady) {
    currentSelectedRaw = currentPwmRaw; currentSelectedChannel = currentPwmChannel;
    currentSelectedStep = currentPwmStep; currentSelectedUs = currentPwmCapturedUs;
    currentSelectedReady = true; currentPwmReady = false;
  }
  const bool pwmBusy = currentPwmBusy;
  if (pwmBusy && motorMicros() - currentPwmStartedUs > CURRENT_DMA_TIMEOUT_US) {
    if (state == State::CLOSED_LOOP && lastCommutationUs == currentPwmSectorUs && currentScanEnabled)
      saveCurrentFailureSnapshot(1); // not an expected cancellation during commutation/STOP
    ADC1->CR2 &= ~ADC_CR2_EXTEN;
    DMA2_Stream0->CR &= ~(DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
    currentPwmBusy = false;
    if (state != State::CLOSED_LOOP || lastCommutationUs != currentPwmSectorUs || !currentScanEnabled) {
      ++currentPwmDropped; // sector/state changed while waiting; not a monitor failure
    } else { currentMonitorOK = false; ++currentFailures; }
  }
  leaveCritical(pwmKey);
  if (pwmBusy) return;
  if (currentBusy) {
    const uint32_t flags = DMA2->LISR;
    if (flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) {
      saveCurrentFailureSnapshot(2);
      DMA2_Stream0->CR &= ~DMA_SxCR_EN;
      currentBusy = false; currentMonitorOK = false; ++currentFailures;
      if (currentCalibrating) abortCurrentZero("DMA error");
      return;
    }
    if (flags & DMA_LISR_TCIF0) {
      // Completion is the DMA flag, not ADC EOC (BEMF ISR clears ADC SR).
      __DMB();
      currentBusy = false;
      DMA2->LIFCR = CURRENT_DMA_CLEAR;
      currentLastScanUs = now;
      if (currentScanEnabled && !motorActive(state)) consumeCurrentScan();
      else currentHasSample = false; // discard asynchronous STOP scan if RUN has begun
    } else if (now - currentScanAtUs > CURRENT_DMA_TIMEOUT_US) {
      saveCurrentFailureSnapshot(3);
      DMA2_Stream0->CR &= ~DMA_SxCR_EN;
      currentBusy = false; currentMonitorOK = false; ++currentFailures;
      if (currentCalibrating) abortCurrentZero("DMA timeout");
      return; // disable monitor only; never alter motor drive from unverified current
    }
    return;
  }
  if (!currentScanEnabled) {
    // Only touch regular control after normal DMA completion; never stop ADC1
    // or injected triggers, and never truncate an in-flight regular sequence.
    if (currentRegularControlsActive) {
      const uint32_t key = enterCritical();
      ADC1->CR1 &= ~ADC_CR1_SCAN;
      ADC1->CR2 &= ~(ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_EXTEN);
      currentRegularControlsActive = false;
      leaveCritical(key);
    }
    // OFF at boot and subsequent polls do not write ADC registers at all.
    return;
  }
  if (motorActive(state)) {
    currentHasSample = false; // mixed/stopped raw values are never reported as RUN currents
    const uint32_t key = enterCritical();
    if (state == State::CLOSED_LOOP && now - currentScanAtUs >= CURRENT_PWM_PERIOD_US) {
      currentPwmRequest = true; currentScanAtUs = now;
    }
    tryStartPwmCurrent();
    leaveCritical(key);
    return; // no asynchronous conversion during ALIGN/STARTUP/RUN
  }
  currentPwmRequest = false;
  if (now - currentScanAtUs < CURRENT_SAMPLE_PERIOD_US) return;
  if (DMA2_Stream0->CR & DMA_SxCR_EN) return;
  DMA2->LIFCR = CURRENT_DMA_CLEAR;
  DMA2_Stream0->NDTR = 3;
  __DMB();
  DMA2_Stream0->CR |= DMA_SxCR_EN;
  currentScanAtUs = now; currentBusy = true;
  // CR2 is shared with interrupt-driven BEMF trigger enable/disable.
  // Protect the short read-modify-write; no polling/Serial inside this section.
  const uint32_t key = enterCritical();
  ADC1->CR2 &= ~(ADC_CR2_EXTEN | ADC_CR2_DMA | ADC_CR2_DDS);
  ADC1->SR = ~(ADC_SR_OVR | ADC_SR_EOC | ADC_SR_STRT);
  ADC1->SMPR1 = (ADC1->SMPR1 & ~0x1FFUL) | (3UL << 0) | (3UL << 3) | (3UL << 6);
  ADC1->SQR1 = 2UL << ADC_SQR1_L_Pos;
  ADC1->SQR3 = 10UL | (11UL << 5) | (12UL << 10);
  ADC1->CR1 |= ADC_CR1_SCAN;
  ADC1->CR2 = (ADC1->CR2 & ~ADC_CR2_EXTEN) | ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_SWSTART;
  currentRegularControlsActive = true;
  leaveCritical(key);
}
void setCurrentScanEnabled(bool enabled) {
  if (motorActive(state)) { Serial.println(F("Rejected: STOP before CSCAN ON/OFF.")); return; }
  if (enabled && !currentMonitorOK) { Serial.println(F("Rejected: current monitor unavailable; reset and investigate.")); return; }
  if (enabled == currentScanEnabled) {
    Serial.println(enabled ? F("CURRENT scan already ON.") : F("CURRENT scan already OFF.")); return;
  }
  if (currentCalibrating) abortCurrentZero("scan mode changed");
  currentScanEnabled = enabled;
  currentHasSample = false; // never present pre-toggle data as a live reading
  currentSelectedReady = false; currentPwmRequest = false;
  currentZeroSkipPending = false;
  // Keep RAM zero across OFF/ON for comparison; it is lost after reset.
  Serial.println(enabled ? F("CURRENT scan ON: asynchronous diagnostics; protection OFF.")
                         : F("CURRENT scan OFF: pending scan drains; no new regular conversions."));
}
void beginCurrentZero() {
  if (!currentMonitorOK) { Serial.println(F("Rejected: current monitor unavailable.")); return; }
  if (!currentScanEnabled) { Serial.println(F("Rejected: send CSCAN ON while stopped before CZERO.")); return; }
  if (motorActive(state)) { Serial.println(F("Rejected: STOP before CZERO.")); return; }
  currentZeroReady = false; currentZeroCount = 0;
  currentZeroStatsReady = false;
  for (uint8_t i = 0; i < 3; ++i) {
    currentZeroMin[i] = 4095; currentZeroMax[i] = 0;
    currentZeroTrimMin[i] = 0; currentZeroTrimMax[i] = 0;
  }
  // Discard any scan started before this command.
  currentZeroSkipPending = currentBusy;
  currentCalibrating = true;
  Serial.println(F("CURRENT ZERO: keep motor stopped/no phase current for 64 scans (~0.64 s)."));
}
void printCurrent() {
  char line[240];
  Serial.println(currentScanEnabled ? F("current scan=ON") : F("current scan=OFF (no live current measurement)"));
  if (motorActive(state)) {
    snprintf(line, sizeof(line), "current monitor=PWM_ON_LOW_SIDE ready=%u busy=%u failures=%lu dma_irq=%lu accepted=%lu dropped=%lu window_miss=%lu protection=OFF\n",
             currentMonitorOK, currentPwmBusy, static_cast<unsigned long>(currentFailures), static_cast<unsigned long>(currentDmaIrqs),
             static_cast<unsigned long>(currentPwmAccepted), static_cast<unsigned long>(currentPwmDropped),
             static_cast<unsigned long>(currentWindowMisses));
    Serial.print(line);
    snprintf(line, sizeof(line), "PWM trigger=TIM3_TRGO_OC1REF timer_ready=%u offset_us=%lu ticks_per_us=%lu arm/done/on_ticks=%lu/%lu/%lu\n",
             currentTriggerTimerOK, static_cast<unsigned long>(CURRENT_PWM_OFFSET_US),
             static_cast<unsigned long>(currentTicksPerUs), static_cast<unsigned long>(currentLastArmTicks),
             static_cast<unsigned long>(currentLastDoneTicks), static_cast<unsigned long>(currentLastOnTicks));
    Serial.print(line);
    if (!currentMonitorOK || !currentScanEnabled || !currentSelectedReady || motorMicros() - currentSelectedUs > 100000UL) {
      Serial.println(F("selected current unavailable/stale: waiting for valid RUN PWM ON window.")); return;
    }
    const int32_t delta = static_cast<int32_t>(currentSelectedRaw) - currentZero[currentSelectedChannel];
    snprintf(line, sizeof(line), "acquisition step=%u low=%c raw=%u age_us=%lu (single channel, not U/V/W vector)\n",
             currentSelectedStep + 1, "UVW"[currentSelectedChannel], currentSelectedRaw,
             static_cast<unsigned long>(motorMicros() - currentSelectedUs));
    Serial.print(line);
    if (!currentZeroReady) { Serial.println(F("zero unavailable: STOP then CZERO.")); return; }
    Serial.print(F("selected signed_delta=")); Serial.print(delta); Serial.print(F(" counts; estimated current="));
    Serial.print(currentDeltaMilliAmps(delta) / 1000.0f, 3);
    Serial.println(F(" A (INA181A1/1mOhm/nominal VDDA3.3V; PWM-qualified diagnostic only; polarity unverified; not RMS/peak trip)"));
    return;
  }
  snprintf(line, sizeof(line), "current monitor=RAW_ASYNC ready=%u sample=%u scans=%lu failures=%lu age_us=%lu zero_ready=%u calibrating=%u progress=%u/64 protection=OFF\n",
           currentMonitorOK, currentHasSample, static_cast<unsigned long>(currentScans),
           static_cast<unsigned long>(currentFailures),
           static_cast<unsigned long>(currentHasSample ? motorMicros() - currentLastScanUs : 0),
           currentZeroReady, currentCalibrating, currentZeroCount);
  Serial.print(line);
  if (!currentMonitorOK || !currentScanEnabled || !currentHasSample) {
    Serial.println(F("raw/delta/amps withheld: monitor failed, scan OFF or waiting for a fresh scan."));
    return;
  }
  snprintf(line, sizeof(line), "current ADC PC0/PC1/PC2 (assumed U/V/W) raw=%u,%u,%u\n",
           currentRaw[0], currentRaw[1], currentRaw[2]); Serial.print(line);
  if (currentZeroReady) {
    snprintf(line, sizeof(line), "zero=%u,%u,%u signed_delta=%ld,%ld,%ld counts (not PWM-synchronized)\n",
             currentZero[0], currentZero[1], currentZero[2],
             static_cast<long>(static_cast<int32_t>(currentRaw[0]) - currentZero[0]),
             static_cast<long>(static_cast<int32_t>(currentRaw[1]) - currentZero[1]),
             static_cast<long>(static_cast<int32_t>(currentRaw[2]) - currentZero[2]));
    Serial.print(line);
    Serial.print(F("estimated signed current U/V/W="));
    for (uint8_t i = 0; i < 3; ++i) {
      if (i) Serial.print(',');
      const int32_t delta = static_cast<int32_t>(currentRaw[i]) - currentZero[i];
      Serial.print(currentDeltaMilliAmps(delta) / 1000.0f, 3);
    }
    Serial.println(F(" A (ASYNC diagnostic only; not RMS, DC-bus current or peak protection)"));
    snprintf(line, sizeof(line), "scale=INA181A1 gain=%lu shunt=%lu mOhm VDDA=%lu mV (assumed; direct ADC input; polarity unverified)\n",
             static_cast<unsigned long>(CURRENT_GAIN), static_cast<unsigned long>(CURRENT_SHUNT_MILLIOHM),
             static_cast<unsigned long>(CURRENT_VDDA_MV));
    Serial.print(line);
  } else Serial.println(F("zero/delta unavailable; STOP, allow current to decay, then CZERO."));
  if (currentZeroStatsReady) printCurrentZeroStats();
}

void configurePhaseAdc() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  // PA0/1/2 analog, no pull. Leave USB PA11/12 and SWD PA13/14 intact.
  GPIOA->MODER |= 0x3FUL;
  GPIOA->PUPDR &= ~0x3FUL;
  __HAL_RCC_ADC1_CLK_ENABLE(); __HAL_RCC_ADC2_CLK_ENABLE(); __HAL_RCC_ADC3_CLK_ENABLE();
  // Independent mode, all three receive TIM1_CC4. Maximum APB2=84MHz ->21MHz.
  ADC->CCR = ADC_CCR_ADCPRE_0;
  ADC_TypeDef *const units[3] = {ADC1, ADC2, ADC3};
  for (uint32_t channel = 0; channel < 3; ++channel) {
    ADC_TypeDef *adc = units[channel];
    adc->CR2 = 0; adc->CR1 = 0;
    adc->SMPR1 = 0;
    adc->SMPR2 = 1UL << (3UL * channel); // sample code 001: 15 ADC cycles
    // For injected length=1, the channel MUST be in JSQ4, result in JDR1.
    adc->JSQR = channel << ADC_JSQR_JSQ4_Pos;
    adc->JOFR1 = 0;
    adc->SR = 0;
    // JEXTSEL=0000: TIM1_CC4; trigger enabled only when a step is driven.
    adc->CR2 = ADC_CR2_ADON;
  }
  ADC3->CR1 = ADC_CR1_JEOCIE;
  NVIC_SetPriority(ADC_IRQn, 2);
  NVIC_ClearPendingIRQ(ADC_IRQn);
  NVIC_EnableIRQ(ADC_IRQn);
}
bool configureTimers() {
  pwmTimer.pause();
  pwmTimer.setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PIN_U_HIN);
  pwmTimer.setMode(2, TIMER_OUTPUT_COMPARE_PWM1, PIN_V_HIN);
  pwmTimer.setMode(3, TIMER_OUTPUT_COMPARE_PWM1, PIN_W_HIN);
  pwmTimer.setMode(4, TIMER_OUTPUT_COMPARE_PWM2); // internal channel, no GPIO
  // Explicitly require all pulse/trigger CCRs to load at hardware rollover.
  TIM1->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
  TIM1->CCMR2 |= TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE;
  pwmTimer.setOverflow(PWM_FREQUENCY_HZ, HERTZ_FORMAT);
  pwmTimer.setCaptureCompare(1, 0); pwmTimer.setCaptureCompare(2, 0);
  pwmTimer.setCaptureCompare(3, 0); pwmTimer.setCaptureCompare(4, 1);
  pwmTimer.attachInterrupt(onPwmPeriod);
  pwmTimer.setInterruptPriority(2, 0);
  pwmTimer.resume();
  // Idle levels LOW even with MOE cleared; no complementary outputs.
  TIM1->CCER &= ~(TIM_CCER_CC1NE | TIM_CCER_CC2NE | TIM_CCER_CC3NE);
  TIM1->CCER |= TIM_CCER_CC4E;
  TIM1->CR2 &= ~(TIM_CR2_OIS1 | TIM_CR2_OIS1N | TIM_CR2_OIS2 | TIM_CR2_OIS2N |
                  TIM_CR2_OIS3 | TIM_CR2_OIS3N | TIM_CR2_OIS4);
  TIM1->BDTR |= TIM_BDTR_OSSI;
  TIM1->BDTR &= ~TIM_BDTR_AOE;
  gatesOff();
  commTimer.pause();
  const uint32_t clockHz = commTimer.getTimerClkFreq();

  if (clockHz < 1000000 || clockHz % 1000000 != 0) return false;
  commTimer.setPrescaleFactor(clockHz / 1000000);
  commTimer.setOverflow(10000, TICK_FORMAT);
  commTimer.attachInterrupt(onCommutationTimer);
  commTimer.setInterruptPriority(2, 0);
  commTimer.resume();
  commTimer.pause();
  TIM2->CR1 |= TIM_CR1_OPM | TIM_CR1_URS;
  TIM2->DIER |= TIM_DIER_UIE;
  // ADC conversion must finish before PWM OFF at the minimum duty.
  const uint32_t adcHz = HAL_RCC_GetPCLK2Freq() / 4UL;

  if (adcHz == 0 || adcHz > 36000000) return false;
  const uint32_t conversionNs = (29UL * 1000000000ULL + adcHz - 1) / adcHz;
  const uint32_t halfOnNs = (1000000000UL / PWM_FREQUENCY_HZ) * MIN_PULSE_DUTY / 200UL;

  return adcHz > 0 && adcHz <= 36000000 && halfOnNs > conversionNs + 300;
}
bool configureWatchdog() {
  RCC->CSR |= RCC_CSR_LSION;
  const uint32_t started = motorMicros();
  while (!(RCC->CSR & RCC_CSR_LSIRDY)) {
    if (motorMicros() - started > 10000) return false;
  }
  // Start the IWDG kernel BEFORE writing PR/RLR and waiting for PVU/RVU.
  // This is the same ordering used by STM32F4 HAL_IWDG_Init. Waiting for
  // update flags with the watchdog kernel stopped can time out forever.
  IWDG->KR = 0xCCCC;
  IWDG->KR = 0x5555;
  IWDG->PR = 4; // /64; about one second at nominal 40kHz LSI
  IWDG->RLR = 625;
  const uint32_t waitAt = motorMicros();
  while (IWDG->SR != 0) {
    if (motorMicros() - waitAt > 100000) return false;
  }
  IWDG->KR = 0xAAAA;
  return true;
}
const char *stateName(State s) {
  switch (s) {
    case State::DISARMED: return "DISARMED";
    case State::ARMED_IDLE: return "ARMED_IDLE";
    case State::ALIGN: return "ALIGN";
    case State::STARTUP: return "STARTUP";
    case State::CLOSED_LOOP: return "CLOSED_LOOP";
    case State::FAULT: return "FAULT";
  }
  return "?";
}
const char *faultName(Fault f) {
  switch (f) {
    case Fault::NONE: return "none";
    case Fault::ADC_TIMEOUT: return "no synchronized ADC samples";
    case Fault::PHASE_SIGNAL: return "phase ON rails invalid; check LOW clamp, bus, drivers and PA0/1/2";
    case Fault::ADC_SATURATED: return "phase ADC near full scale; check divider voltage";
    case Fault::STARTUP_TIMEOUT: return "startup could not lock BEMF";
    case Fault::BEMF_LOST: return "no valid BEMF crossing before sector deadline";
    case Fault::SPEED_LIMIT: return "BEMF cycle average below 600 us for 2 cycles or emergency estimate below 500 us; reduce duty";
    case Fault::COMM_LATE: return "BEMF confirmation too late for commutation; reduce duty";
    case Fault::CLOCK: return "timer/ADC clock or watchdog initialization failed";
    case Fault::CURRENT_MONITOR: return "current acquisition failed; send CDIAG before reset";
    case Fault::APP_TIMEOUT: return "application heartbeat timeout; outputs latched OFF";
  }
  return "?";
}
const char *faultCode(Fault f) {
  switch (f) {
    case Fault::NONE: return "none";
    case Fault::ADC_TIMEOUT: return "adc_timeout";
    case Fault::PHASE_SIGNAL: return "phase_signal";
    case Fault::ADC_SATURATED: return "adc_saturated";
    case Fault::STARTUP_TIMEOUT: return "startup_timeout";
    case Fault::BEMF_LOST: return "bemf_lost";
    case Fault::SPEED_LIMIT: return "speed_limit";
    case Fault::COMM_LATE: return "comm_late";
    case Fault::CLOCK: return "clock";
    case Fault::CURRENT_MONITOR: return "current_monitor";
    case Fault::APP_TIMEOUT: return "app_timeout";
  }
  return "unknown";
}
size_t formatStatus(char *line, size_t capacity) {
  const uint32_t key = enterCritical();
  const State current = state;
  const Fault reason = fault;
  const uint32_t forced = forcedPeriodUs, measured = measuredPeriodUs;
  const uint32_t initial = initialPeriodUs;
  const uint16_t ramp = startupRampDivisor;
  const uint16_t dutyRamp = dutyRampPctPerS;
  const uint32_t samples = adcSamples, incomplete = skippedAdcSets;
  const uint32_t rejected = rejectedCrossings, lowBad = lowClampRejected;
  const uint32_t closed = closedLoopSteps;
  const uint8_t step = activeStep, locks = lockCount, peak = peakLockCount;
  const uint16_t raw[3] = {latestRaw[0], latestRaw[1], latestRaw[2]};
  const uint16_t neutral = latestNeutral, average = effectiveDutyPermille;
  const uint16_t pulse = physicalDutyPermille;
  const bool valid = latestSignalValid;
  const uint32_t now = motorMicros();
  const uint32_t phaseAgeUs = now - lastAdcUs;
  const bool hbFresh = appHeartbeatFresh(now);
  const uint32_t hbAgeMs = appHeartbeatSeen ? (now - lastAppHeartbeatUs) / 1000UL : 0xFFFFFFFFUL;
  const Fault currentFault = fault;
  leaveCritical(key);
  (void)reason;
  const int written = snprintf(line, capacity,
           "OK STATUS fw=v33-proto1 protocol=1 state=%s fault=%s pwm_hz=%lu duty_target_pct=%u duty_effective_pct_tenths=%u duty_pulse_pct_tenths=%u duty_ramp_pct_per_s=%u direction=%s target_period_us=%lu startup_initial_period_us=%lu startup_ramp_divisor=%u forced_period_us=%lu measured_period_us=%lu bemf_lock_count=%u bemf_lock_required=%u bemf_lock_peak=%u closed_loop_steps=%lu step=%u floating_phase=%c phase_u_adc_counts=%u phase_v_adc_counts=%u phase_w_adc_counts=%u neutral_adc_counts=%u phase_valid=%u phase_age_us=%lu init_ok=%u timer_ok=%u iwdg_ok=%u adc_samples=%lu adc_incomplete=%lu adc_rejected=%lu low_clamp_rejected=%lu heartbeat=%s heartbeat_age_ms=%lu heartbeat_timeout_ms=%lu current_scan=%s current_protection=OFF current_data=UNVERIFIED\n",
           stateName(current), faultCode(currentFault), static_cast<unsigned long>(PWM_FREQUENCY_HZ),
           dutyPercent, average, pulse, dutyRamp, forwardDirection ? "fwd" : "rev",
           static_cast<unsigned long>(targetPeriodUs), static_cast<unsigned long>(initial), ramp,
           static_cast<unsigned long>(forced),
           static_cast<unsigned long>(measured), locks, LOCK_CROSSES, peak,
           static_cast<unsigned long>(closed), step + 1, "UVW"[STEPS[step].floating],
           raw[0], raw[1], raw[2], neutral, valid, static_cast<unsigned long>(phaseAgeUs), peripheralsReady, timerInitOK,
           watchdogInitOK, static_cast<unsigned long>(samples), static_cast<unsigned long>(incomplete),
           static_cast<unsigned long>(rejected), static_cast<unsigned long>(lowBad),
           hbFresh ? "OK" : "STALE", static_cast<unsigned long>(hbAgeMs),
           static_cast<unsigned long>(APP_HEARTBEAT_TIMEOUT_MS),
           currentScanEnabled ? "ON" : "OFF");
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}
void printStatus() {
  char line[1200];
  const size_t length = formatStatus(line, sizeof(line));
  if (length) Serial.write(reinterpret_cast<const uint8_t *>(line), length);
}
void queueStatusReply() {
  telemetryReplyLength = formatStatus(telemetryReplyBuffer, sizeof(telemetryReplyBuffer));
  telemetryReplyOffset = 0;
}
void queueHelloReply() {
  static const char hello[] =
      "OK HELLO fw=v33-proto1 protocol=1 board=STM32F405 transport=USB_CDC caps=arm,run,stop,status,diag,duty,period,dir,startup_profile,duty_response,phase_stream,scope_capture,scope_live unavailable=v_in,current_motor,current_in,rpm,temp_mos,temp_motor,ah,wh,tacho heartbeat_timeout_ms=1500 stream_interval_min_ms=100 stream_interval_max_ms=1000 phase_adc_unit=counts phase_voltage_conversion=UNVERIFIED current_data=UNVERIFIED current_protection=OFF\n";
  static_assert(sizeof(hello) <= sizeof(telemetryReplyBuffer), "HELLO exceeds reply buffer");
  memcpy(telemetryReplyBuffer, hello, sizeof(hello));
  telemetryReplyLength = sizeof(hello) - 1;
  telemetryReplyOffset = 0;
}
void serviceTelemetryReply() {
  if (telemetryReplyOffset >= telemetryReplyLength) return;
  const int available = Serial.availableForWrite();
  if (available <= 0) return;
  const size_t remaining = telemetryReplyLength - telemetryReplyOffset;
  const size_t count = min(remaining, static_cast<size_t>(available));
  // A single write never exceeds the free USB queue space, so it cannot wait.
  telemetryReplyOffset += Serial.write(
      reinterpret_cast<const uint8_t *>(telemetryReplyBuffer + telemetryReplyOffset), count);
  if (telemetryReplyOffset == telemetryReplyLength) {
    telemetryReplyOffset = 0;
    telemetryReplyLength = 0;
  }
}
void serviceScopeRead() {
  if (!scopeSending) return;
  if (motorMicros() - scopeSendStartedUs > 8000000UL) {
    if (scopeLiveFrame) { scopeLive = false; scopeReady = false; scopeCount = 0; }
    scopeSending = false; scopeSendIndex = 0; scopeLiveFrame = false; scopeFrameBegin = false; return;
  }
  const int available = Serial.availableForWrite();
  if (available <= 0) return;
  int written;
  if (scopeFrameBegin) {
    written = snprintf(scopeLine, sizeof(scopeLine), "OK SCOPE FRAME BEGIN count=%u\n", scopeCount);
  } else if (scopeSendIndex < scopeCount) {
    const ScopeSample &s = scopeSamples[scopeSendIndex];
    written = snprintf(scopeLine, sizeof(scopeLine),
        "OK SCOPE SAMPLE i=%u t_us=%lu u=%u v=%u w=%u f=%c step=%u\n",
        scopeSendIndex, static_cast<unsigned long>(s.tUs), s.u, s.v, s.w,
        "UVW"[s.floating], s.step);
  } else {
    written = snprintf(scopeLine, sizeof(scopeLine), scopeLiveFrame ?
        "OK SCOPE FRAME END count=%u\n" : "OK SCOPE END count=%u\n", scopeCount);
  }
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(scopeLine) ||
      available < written + (scopeLiveFrame ? 32 : 0)) return;
  if (Serial.write(reinterpret_cast<const uint8_t *>(scopeLine), written) != static_cast<size_t>(written)) return;
  if (scopeFrameBegin) scopeFrameBegin = false;
  else if (scopeSendIndex < scopeCount) ++scopeSendIndex;
  else {
    if (scopeLiveFrame) { scopeReady = false; scopeCount = 0; }
    scopeSending = false; scopeSendIndex = 0; scopeLiveFrame = false;
    scopeLiveNextUs = motorMicros() + SCOPE_LIVE_FRAME_INTERVAL_US;
  }
}
size_t formatPhaseSample(char *line, size_t capacity) {
  const uint32_t key = enterCritical();
  const uint32_t now = motorMicros();
  const uint32_t sampleUs = lastAdcUs;
  const uint16_t rawU = latestRaw[0], rawV = latestRaw[1], rawW = latestRaw[2];
  const uint8_t floating = latestFloatingIndex;
  const bool valid = latestSignalValid;
  const uint32_t ageUs = now - sampleUs;
  leaveCritical(key);
  const int written = snprintf(line, capacity,
      "OK PHASE t_us=%lu u_adc_counts=%u v_adc_counts=%u w_adc_counts=%u phase_valid=%u phase_age_us=%lu f=%c\n",
      static_cast<unsigned long>(sampleUs), rawU, rawV, rawW, valid,
      static_cast<unsigned long>(ageUs), "UVW"[floating]);
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}
void servicePhaseStream() {
  if (!phaseStreamEnabled || telemetryReplyLength || (scopeSending && !scopeLiveFrame)) return;
  const uint32_t now = motorMicros();
  if (!due(now, nextPhaseStreamUs)) return;
  nextPhaseStreamUs = now + static_cast<uint32_t>(phaseStreamIntervalMs) * 1000UL;
  const size_t length = formatPhaseSample(phaseStreamBuffer, sizeof(phaseStreamBuffer));
  if (!length) return;
  const int available = Serial.availableForWrite();
  // Queue a whole line atomically or drop this sample; never block motor timing.
  if (available < 0 || static_cast<size_t>(available) < length) return;
  Serial.write(reinterpret_cast<const uint8_t *>(phaseStreamBuffer), length);
}
void printDiagnosis() {
  printStatus();
  uint16_t lowBad[6], crosses[6], centered[6], position[6];
  const uint32_t key = enterCritical();
  for (uint8_t i = 0; i < 6; ++i) {
    lowBad[i] = lowRejectedByStep[i];
    crosses[i] = crossingsByStep[i];
    centered[i] = centeredByStep[i];
    position[i] = crossingPosition[i];
  }
  const uint32_t sectors = sectorCount;
  const uint32_t validSamples = sectorValidSamples, invalidSamples = sectorInvalidSamples;
  const int32_t errorMin = sectorErrorMin, errorMax = sectorErrorMax;
  const bool before = seenBefore, candidate = candidateValid;
  const uint8_t confirmations = afterCount, rejection = sectorRejectReason;
  const uint32_t crossInto = candidate ? candidateUs - lastCommutationUs : 0;
  const uint32_t edgeInterval = sectorEdgeIntervalUs, edgeExpected = sectorEdgeExpectedUs;
  const bool sequential = sectorEdgeSequential;
  const uint32_t confirmDelay = sectorConfirmDelayUs, late = lateConfirmedCrossings;
  const int32_t lead = sectorCommLeadUs, minLead = minimumCommLeadUs;
  const uint32_t negativeSamples = sectorNegativeSamples, firstValid = sectorFirstValidIntoUs;
  const uint8_t peakBefore = sectorPeakBeforeCount;
  const uint32_t speedAverage = speedCycleAverageUs, speedSum = speedCycleSumUs;
  const uint8_t speedSamples = speedCycleSamples, fastCycles = speedFastCycles;
  leaveCritical(key);
  char line[220];
  snprintf(line, sizeof(line), "sector valid/invalid=%lu/%lu error_min/max=%ld/%ld before=%u candidate=%u candidate_into_us=%lu after=%u reject=%u (0=none 1=early 2=late 3=interval)\n",
           static_cast<unsigned long>(validSamples), static_cast<unsigned long>(invalidSamples),
           static_cast<long>(errorMin == INT32_MAX ? 0 : errorMin),
           static_cast<long>(errorMax == INT32_MIN ? 0 : errorMax), before, candidate,
           static_cast<unsigned long>(crossInto), confirmations, rejection);
  Serial.print(line);
  snprintf(line, sizeof(line), "edge interval/expected_us=%lu/%lu sequential=%u (latest attempted interval in this sector)\n",
           static_cast<unsigned long>(edgeInterval), static_cast<unsigned long>(edgeExpected), sequential);
  Serial.print(line);
  Serial.print(F("startup sectors=")); Serial.println(sectors);
  snprintf(line, sizeof(line), "speed guard: cycle_average_us=%lu partial_sum_us=%lu samples=%u/6 fast_cycles=%u/%u emergency_us=%lu\n",
           static_cast<unsigned long>(speedAverage), static_cast<unsigned long>(speedSum),
           speedSamples, fastCycles, SPEED_GUARD_CYCLES, static_cast<unsigned long>(HARD_FAST_PERIOD_US));
  Serial.print(line);
  snprintf(line, sizeof(line), "sector pre-cross: first_valid_into_us=%lu negative_samples=%lu peak_before=%u\n",
           static_cast<unsigned long>(firstValid), static_cast<unsigned long>(negativeSamples), peakBefore);
  Serial.print(line);
  snprintf(line, sizeof(line), "RUN timing: limit_us=%lu confirm_delay_us=%lu comm_lead_us=%ld min_lead_us=%ld late=%lu (min=0 before first RUN edge)\n",
           static_cast<unsigned long>(MIN_PERIOD_US), static_cast<unsigned long>(confirmDelay),
           static_cast<long>(lead), static_cast<long>(minLead == INT32_MAX ? 0 : minLead),
           static_cast<unsigned long>(late));
  Serial.print(line);
  Serial.println(F("step H L F crossing centered low_bad cross_permille"));
  for (uint8_t i = 0; i < 6; ++i) {
    Serial.print(i + 1); Serial.print(' ');
    Serial.print("UVW"[STEPS[i].high]); Serial.print(' ');
    Serial.print("UVW"[STEPS[i].low]); Serial.print(' ');
    Serial.print("UVW"[STEPS[i].floating]); Serial.print(' ');
    Serial.print(crosses[i]); Serial.print(' ');
    Serial.print(centered[i]); Serial.print(' ');
    Serial.print(lowBad[i]); Serial.print(' ');
    Serial.println(position[i]);
  }
}
void printHelp() {
  Serial.println(F("Commands: HELLO | HEARTBEAT | ARM | RUN | STOP | STATUS | DIAG | HELP"));
  Serial.println(F("HEARTBEAT required before ARM/RUN; timeout is 1500 ms."));
  Serial.println(F("Phase telemetry only: STREAM ON [100..1000 ms] | STREAM OFF; default interval 100 ms."));
  Serial.println(F("Scope: SCOPE LIVE ON/OFF in CLOSED_LOOP for bounded live frames; SCOPE CAPTURE then STOP, SCOPE READ for snapshot."));
  Serial.println(F("Duty: 20 or duty 20 sets magnitude; +20/duty +20=forward, -20/duty -20=reverse."));
  Serial.println(F("Bare signed duty changes direction only while stopped; unsigned duty keeps dir."));
  Serial.println(F("Signed duty changes direction only while stopped; while running its sign must match current direction."));
  Serial.println(F("PWM: below 16% uses pulse density; at/above 16% uses fractional width every carrier cycle."));
  Serial.println(F("Idle only: period 600..12000 (target us) | startperiod 12000..30000 (initial step us) | ramp 150..300 (lower=faster) | response 10..30 (duty percent/s) | dir fwd|rev"));
  Serial.println(F("Experimental RUN limit: 600 us/sector; minimum commutation lead 16 us. Validate duty 50 progressively."));
  Serial.println(F("run: ALIGN -> STARTUP -> CLOSED_LOOP. Current protection OFF."));
  Serial.println(F("Startup: ALIGN 200 ms, 2..5%; cap 5%; default duty ramp 20 points/s; initial 20000 us; default ramp divisor 200; lock requires 6."));
  Serial.println(F("CSCAN ON/OFF: STOP first; default OFF. RUN CURRENT: PWM ON selected LOW; no current trip."));
  Serial.println(F("CZERO: CSCAN ON, stopped/no current; 64-scan RAM zero."));
  Serial.println(F("CTEST: STOP DC calibration, 64 fresh scans; CRESULT repeats results."));
  Serial.println(F("CREF U|V|W signed_mA: compare completed CTEST to shunt reference, nonzero +/-5000 max; no scale change."));
  Serial.println(F("CDIAG: saved pre-cleanup failure registers + live timer/ADC/DMA state; use before reset."));
}
void startController() {
  if (!appHeartbeatFresh(motorMicros())) { Serial.println(F("ERR RUN reason=heartbeat_stale")); return; }
  if (!peripheralsReady) { Serial.println(F("ERR RUN reason=peripheral_initialization_failed")); return; }
  if (currentCalibrating) { Serial.println(F("ERR RUN reason=current_zero_in_progress")); return; }
  if (currentTestActive) { Serial.println(F("ERR RUN reason=current_test_in_progress")); return; }
  const uint32_t key = enterCritical();
  if (state != State::ARMED_IDLE) {
    leaveCritical(key); Serial.println(F("ERR RUN reason=arm_required")); return;
  }
  gatesOff(); disableAdcTrigger();
  forcedPeriodUs = max(static_cast<uint32_t>(initialPeriodUs), static_cast<uint32_t>(targetPeriodUs));
  effectiveDutyPermille = ALIGN_START_PERMILLE;
  driveStartedUs = motorMicros();
  lastDutyRampUs = driveStartedUs;
  measuredPeriodUs = forcedPeriodUs;
  sectorCount = 0; startupAtUs = 0;
  lockCount = 0; peakLockCount = 0; previousCrossValid = false;
  lowClampRejected = 0;
  for (uint8_t i = 0; i < 6; ++i) {
    lowRejectedByStep[i] = 0;
    crossingsByStep[i] = 0;
    centeredByStep[i] = 0;
    crossingPosition[i] = 0;
  }
  closedLoopSteps = 0; invalidPhaseSamples = 0;
  currentSelectedReady = false; currentPwmRequest = false;
  minimumCommLeadUs = INT32_MAX; lateConfirmedCrossings = 0;
  resetSpeedGuard();
  badSignalCount = 0; crossingCount = 0; rejectedCrossings = 0;
  adcSamples = 0; skippedAdcSets = 0; latestSignalValid = false;
  lockNotice = false; fault = Fault::NONE; reportedFault = Fault::NONE;
  lastAdcUs = motorMicros();
  leaveCritical(key);
  // Bootstrap precharge before any high-side pulse; interrupts remain active.
  GPIOB->BSRR = LIN_MASK;
  delay(2);
  GPIOB->BSRR = LIN_MASK << 16;
  const uint32_t secondKey = enterCritical();
  state = State::ALIGN;
  beginStep(forwardDirection ? 0 : 5);
  leaveCritical(secondKey);
  Serial.println(F("OK RUN state=ALIGN sequence=ALIGN_STARTUP_CLOSED_LOOP"));
}
bool parseNumber(const char *arg, long &value) {
  if (!arg || !*arg) return false;
  char *end = nullptr;
  value = strtol(arg, &end, 10);
  return end != arg && *end == '\0';
}
void handleCommand(char *line) {
  for (char *p = line; *p; ++p) *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
  char *command = strtok(line, " \t");
  char *arg = strtok(nullptr, " \t");
  if (!command) return;
  if (!strcmp(command, "hello")) {
    if (arg) { Serial.println(F("ERR HELLO reason=unexpected_argument")); return; }
    queueHelloReply();
    return;
  }
  if (!strcmp(command, "heartbeat")) {
    if (arg) { Serial.println(F("ERR HEARTBEAT reason=unexpected_argument")); return; }
    lastAppHeartbeatUs = motorMicros();
    appHeartbeatSeen = true;
    Serial.println(F("OK HEARTBEAT lease_ms=1500"));
    return;
  }
  if (!strcmp(command, "scope")) {
    char *extra = strtok(nullptr, " \t");
    if (arg && !strcmp(arg, "live")) {
      if (!extra || strtok(nullptr, " \t") || (strcmp(extra, "on") && strcmp(extra, "off"))) {
        Serial.println(F("ERR SCOPE reason=expected_LIVE_ON_or_OFF")); return;
      }
      if (!strcmp(extra, "off")) {
        const uint32_t key = enterCritical();
        scopeLive = false; scopeCapturing = false; scopeReady = false; scopeCount = 0;
        leaveCritical(key);
        if (scopeLiveFrame) { scopeSending = false; scopeLiveFrame = false; scopeFrameBegin = false; scopeSendIndex = 0; }
        Serial.println(F("OK SCOPE state=LIVE_OFF")); return;
      }
      const uint32_t key = enterCritical();
      const bool allowed = state == State::CLOSED_LOOP && !scopeCapturing && !scopeSending;
      if (allowed) {
        scopeLive = true; scopeCount = 0; scopeReady = false; scopeDecimation = 0;
        scopeStartedUs = motorMicros(); scopeCapturing = true;
      }
      leaveCritical(key);
      if (!allowed) Serial.println(F("ERR SCOPE reason=closed_loop_required_or_busy"));
      else Serial.println(F("OK SCOPE state=LIVE_ON points_per_frame=256 min_frame_interval_ms=300"));
      return;
    }
    if (!arg || extra) { Serial.println(F("ERR SCOPE reason=expected_CAPTURE_STATUS_READ_or_CLEAR")); return; }
    if (!strcmp(arg, "capture")) {
      const uint32_t key = enterCritical();
      const bool allowed = state == State::CLOSED_LOOP && !scopeLive && !scopeCapturing && !scopeSending;
      if (allowed) {
        scopeCount = 0; scopeReady = false; scopeDecimation = 0;
        scopeStartedUs = motorMicros(); scopeCapturing = true;
      }
      leaveCritical(key);
      if (!allowed) Serial.println(F("ERR SCOPE reason=closed_loop_required_or_busy"));
      else Serial.println(F("OK SCOPE state=CAPTURING max=512 max_duration_ms=2000"));
      return;
    }
    if (!strcmp(arg, "status")) {
      const uint32_t key = enterCritical();
      const bool live = scopeLive, capturing = scopeCapturing, ready = scopeReady;
      const uint16_t count = scopeCount;
      leaveCritical(key);
      const int written = snprintf(telemetryReplyBuffer, sizeof(telemetryReplyBuffer),
          "OK SCOPE state=%s count=%u max=512\n",
          live ? "LIVE" : capturing ? "CAPTURING" : ready ? "READY" : "EMPTY", count);
      telemetryReplyLength = written > 0 && static_cast<size_t>(written) < sizeof(telemetryReplyBuffer)
          ? static_cast<size_t>(written) : 0;
      telemetryReplyOffset = 0;
      return;
    }
    if (!strcmp(arg, "read")) {
      if (motorActive(state) || scopeCapturing || scopeLive) { Serial.println(F("ERR SCOPE reason=stop_before_read")); return; }
      if (!scopeReady || !scopeCount) { Serial.println(F("ERR SCOPE reason=no_capture")); return; }
      scopeSending = true; scopeLiveFrame = false; scopeFrameBegin = false;
      scopeSendIndex = 0; scopeSendStartedUs = motorMicros();
      Serial.print(F("OK SCOPE state=READING count=")); Serial.println(scopeCount); return;
    }
    if (!strcmp(arg, "clear")) {
      if (motorActive(state) || scopeCapturing) { Serial.println(F("ERR SCOPE reason=stop_before_clear")); return; }
      scopeCount = 0; scopeReady = false;
      Serial.println(F("OK SCOPE state=EMPTY count=0")); return;
    }
    Serial.println(F("ERR SCOPE reason=invalid_mode")); return;
  }
  if (!strcmp(command, "stream")) {
    if (!arg) { Serial.println(F("ERR STREAM reason=mode_required expected=ON_or_OFF")); return; }
    char *intervalArg = strtok(nullptr, " \t");
    char *extraArg = strtok(nullptr, " \t");
    if (!strcmp(arg, "off")) {
      if (intervalArg || extraArg) { Serial.println(F("ERR STREAM reason=unexpected_argument")); return; }
      phaseStreamEnabled = false;
      Serial.println(F("OK STREAM kind=phase state=OFF"));
      return;
    }
    if (strcmp(arg, "on") || extraArg) {
      Serial.println(F("ERR STREAM reason=invalid_mode_or_argument expected=ON_or_OFF"));
      return;
    }
    uint16_t interval = STATUS_STREAM_DEFAULT_MS;
    if (intervalArg) {
      long requested = 0;
      if (!parseNumber(intervalArg, requested) || requested < STATUS_STREAM_MIN_MS ||
          requested > STATUS_STREAM_MAX_MS) {
        Serial.print(F("ERR STREAM reason=interval_range min_ms=")); Serial.print(STATUS_STREAM_MIN_MS);
        Serial.print(F(" max_ms=")); Serial.println(STATUS_STREAM_MAX_MS);
        return;
      }
      interval = static_cast<uint16_t>(requested);
    }
    phaseStreamIntervalMs = interval;
    phaseStreamEnabled = true;
    nextPhaseStreamUs = motorMicros();
    Serial.print(F("OK STREAM kind=phase state=ON interval_ms=")); Serial.println(phaseStreamIntervalMs);
    return;
  }
  if (!strcmp(command, "ctest") || !strcmp(command, "cresult")) {
    if (arg) { Serial.println(F("Invalid: CTEST/CRESULT take no arguments.")); return; }
    if (!strcmp(command, "ctest")) beginCurrentTest(); else printCurrentTest();
    return;
  }
  if (!strcmp(command, "cref")) {
    char *valueArg = strtok(nullptr, " \t");
    long value = 0;
    if (!arg || strlen(arg) != 1 || !strchr("uvw", arg[0]) ||
        !parseNumber(valueArg, value) || value == 0 || value < -5000 || value > 5000 || strtok(nullptr, " \t")) {
      Serial.println(F("Invalid: CREF U|V|W signed_mA; -5000..5000, nonzero.")); return;
    }
    compareCurrentReference(arg[0] == 'u' ? 0 : (arg[0] == 'v' ? 1 : 2), value); return;
  }
  if (!strcmp(command, "cdiag")) {
    if (arg) { Serial.println(F("Invalid: CDIAG takes no arguments.")); return; }
    printCurrentHardwareDiagnosis(); return;
  }
  if (!strcmp(command, "cscan")) {
    if (!arg || (strcmp(arg, "on") && strcmp(arg, "off")) || strtok(nullptr, " \t")) {
      Serial.println(F("Invalid: CSCAN ON or CSCAN OFF.")); return;
    }
    setCurrentScanEnabled(!strcmp(arg, "on")); return;
  }
  if (!strcmp(command, "current") || !strcmp(command, "czero")) {
    if (arg) { Serial.println(F("Invalid: CURRENT and CZERO take no arguments.")); return; }
    if (!strcmp(command, "current")) printCurrent();
    else if (currentTestActive) Serial.println(F("Rejected: wait for CTEST before CZERO."));
    else { currentTestReady = false; beginCurrentZero(); }
    return;
  }
  const bool dutyCommand = !strcmp(command, "duty");
  const bool bareSignedDuty = !arg && (command[0] == '+' || command[0] == '-') &&
      command[1] >= '0' && command[1] <= '9';
  if (dutyCommand || bareSignedDuty) {
    const char *dutyArg = dutyCommand ? arg : command;
    long value = 0;
    if (!parseNumber(dutyArg, value) || value < -static_cast<long>(MAX_DUTY) ||
        value > static_cast<long>(MAX_DUTY) || value == 0 ||
        (value > -static_cast<long>(MIN_DUTY) && value < static_cast<long>(MIN_DUTY)) ||
        (dutyCommand && strtok(nullptr, " \t") != nullptr)) {
      Serial.print(F("ERR DUTY reason=range expected=5..100_pct current_target_pct="));
      Serial.print(dutyPercent); Serial.print(F(" direction="));
      Serial.println(forwardDirection ? F("fwd") : F("rev")); return;
    }
    const bool signedDirection = dutyArg[0] == '-' || dutyArg[0] == '+';
    const bool requestedForward = value > 0;
    const uint8_t magnitude = static_cast<uint8_t>(value < 0 ? -value : value);
    const uint32_t key = enterCritical();
    if (signedDirection && requestedForward != forwardDirection && motorActive(state)) {
      leaveCritical(key);
      Serial.println(F("ERR DUTY reason=stop_before_reverse"));
      return;
    }
    if (signedDirection) forwardDirection = requestedForward;
    dutyPercent = magnitude;
    leaveCritical(key);
    Serial.print(F("OK DUTY target_pct=")); Serial.print(magnitude);
    Serial.print(F(" direction=")); Serial.print(forwardDirection ? F("fwd") : F("rev"));
    Serial.print(F(" ramp_pct_per_s=")); Serial.println(dutyRampPctPerS);
    return;
  }
  if (!strcmp(command, "status")) {
    if (arg) { Serial.println(F("ERR STATUS reason=unexpected_argument")); return; }
    queueStatusReply(); return;
  }
  if (!strcmp(command, "diag")) {
    if (arg) { Serial.println(F("ERR DIAG reason=unexpected_argument")); return; }
    printDiagnosis(); return;
  }
  if (!strcmp(command, "help")) {
    if (arg) { Serial.println(F("ERR HELP reason=unexpected_argument")); return; }
    printHelp(); return;
  }
  if (!strcmp(command, "run")) {
    if (arg) { Serial.println(F("ERR RUN reason=unexpected_argument")); return; }
    startController(); return;
  }
  if (!strcmp(command, "stop")) {
    if (arg) { Serial.println(F("ERR STOP reason=unexpected_argument")); return; }
    const uint32_t key = enterCritical();
    if (state == State::FAULT) gatesOff();
    else stopNow(Fault::NONE);
    const State stoppedState = state;
    const Fault stoppedFault = fault;
    leaveCritical(key);
    Serial.print(F("OK STOP state=")); Serial.print(stateName(stoppedState));
    Serial.print(F(" fault=")); Serial.print(faultCode(stoppedFault)); Serial.println(F(" gates=OFF")); return;
  }
  if (!strcmp(command, "arm")) {
    if (arg) { Serial.println(F("ERR ARM reason=unexpected_argument")); return; }
    if (!appHeartbeatFresh(motorMicros())) { Serial.println(F("ERR ARM reason=heartbeat_stale")); return; }
    const uint32_t key = enterCritical();
    const bool initFailed = !peripheralsReady;
    const bool running = motorActive(state);
    const bool rejected = initFailed || running;
    if (!rejected) { gatesOff(); fault = Fault::NONE; state = State::ARMED_IDLE; reportedFault = Fault::NONE; }
    leaveCritical(key);
    if (initFailed) {
      Serial.println(F("ERR ARM reason=peripheral_initialization_failed"));
      printStatus();
    } else if (running) {
      Serial.println(F("ERR ARM reason=motor_active"));
    } else Serial.println(F("OK ARM state=ARMED_IDLE gates=OFF"));
    return;
  }
  const uint32_t key = enterCritical();
  if (motorActive(state)) {
    leaveCritical(key); Serial.println(F("ERR PARAM reason=stop_before_change")); return;
  }
  char *extraArg = strtok(nullptr, " \t");
  long value = 0;
  bool accepted = false;
  if (!strcmp(command, "period") && parseNumber(arg, value) &&
             !extraArg && value >= static_cast<long>(MIN_PERIOD_US) && value <= static_cast<long>(MAX_TARGET_US)) {
    targetPeriodUs = static_cast<uint32_t>(value); accepted = true;
  } else if (!strcmp(command, "startperiod") && parseNumber(arg, value) && !extraArg &&
             value >= static_cast<long>(MIN_START_PERIOD_US) && value <= static_cast<long>(MAX_START_PERIOD_US)) {
    initialPeriodUs = static_cast<uint32_t>(value); accepted = true;
  } else if (!strcmp(command, "ramp") && parseNumber(arg, value) && !extraArg &&
             value >= MIN_STARTUP_RAMP_DIVISOR && value <= MAX_STARTUP_RAMP_DIVISOR) {
    startupRampDivisor = static_cast<uint16_t>(value); accepted = true;
  } else if (!strcmp(command, "response") && parseNumber(arg, value) && !extraArg &&
             value >= MIN_DUTY_RAMP_PCT_PER_S && value <= MAX_DUTY_RAMP_PCT_PER_S) {
    dutyRampPctPerS = static_cast<uint16_t>(value);
    dutyRampTickUs = 100000UL / dutyRampPctPerS;
    accepted = true;
  } else if (!strcmp(command, "dir") && arg && !extraArg && (!strcmp(arg, "fwd") || !strcmp(arg, "rev"))) {
    forwardDirection = !strcmp(arg, "fwd"); accepted = true;
  }
  leaveCritical(key);
  if (accepted && !strcmp(command, "period")) {
    Serial.print(F("OK PARAM name=period target_period_us=")); Serial.println(targetPeriodUs);
  } else if (accepted && !strcmp(command, "startperiod")) {
    Serial.print(F("OK PARAM name=startperiod startup_initial_period_us=")); Serial.println(initialPeriodUs);
  } else if (accepted && !strcmp(command, "ramp")) {
    Serial.print(F("OK PARAM name=ramp startup_ramp_divisor=")); Serial.println(startupRampDivisor);
  } else if (accepted && !strcmp(command, "response")) {
    Serial.print(F("OK PARAM name=response duty_ramp_pct_per_s=")); Serial.println(dutyRampPctPerS);
  } else if (accepted) {
    Serial.print(F("OK PARAM name=direction direction="));
    Serial.println(forwardDirection ? F("fwd") : F("rev"));
  } else if (!strcmp(command, "period")) {
    Serial.print(F("ERR PARAM name=period reason=range min_us=")); Serial.print(MIN_PERIOD_US);
    Serial.print(F(" max_us=")); Serial.print(MAX_TARGET_US); Serial.print(F(" current_us="));
    Serial.println(targetPeriodUs);
  } else if (!strcmp(command, "startperiod")) {
    Serial.print(F("ERR PARAM name=startperiod reason=range min_us=")); Serial.print(MIN_START_PERIOD_US);
    Serial.print(F(" max_us=")); Serial.print(MAX_START_PERIOD_US);
    Serial.print(F(" current_us=")); Serial.println(initialPeriodUs);
  } else if (!strcmp(command, "ramp")) {
    Serial.print(F("ERR PARAM name=ramp reason=range min=")); Serial.print(MIN_STARTUP_RAMP_DIVISOR);
    Serial.print(F(" max=")); Serial.print(MAX_STARTUP_RAMP_DIVISOR);
    Serial.print(F(" current=")); Serial.println(startupRampDivisor);
  } else if (!strcmp(command, "response")) {
    Serial.print(F("ERR PARAM name=response reason=range min_pct_per_s=")); Serial.print(MIN_DUTY_RAMP_PCT_PER_S);
    Serial.print(F(" max_pct_per_s=")); Serial.print(MAX_DUTY_RAMP_PCT_PER_S);
    Serial.print(F(" current_pct_per_s=")); Serial.println(dutyRampPctPerS);
  } else if (!strcmp(command, "dir")) {
    Serial.print(F("ERR PARAM name=direction reason=invalid_value current="));
    Serial.println(forwardDirection ? F("fwd") : F("rev"));
  } else Serial.println(F("ERR PARAM reason=unsupported_command"));
}
void pollSerial() {
  // Bound input work so a flood of serial data cannot starve supervision.
  for (uint8_t budget = 0; budget < 24 && Serial.available(); ++budget) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      if (!commandOverflow) { commandBuffer[commandLength] = '\0'; handleCommand(commandBuffer); }
      else Serial.println(F("Command too long; discarded."));
      commandLength = 0; commandOverflow = false;
      if (telemetryReplyLength || scopeSending) break;
    } else if (!commandOverflow && commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = c;
    } else commandOverflow = true;
  }
}
void setup() {
  // Drive all six inputs LOW before touching timer/ADC configuration.
  pinMode(PIN_U_HIN, OUTPUT); digitalWrite(PIN_U_HIN, LOW);
  pinMode(PIN_V_HIN, OUTPUT); digitalWrite(PIN_V_HIN, LOW);
  pinMode(PIN_W_HIN, OUTPUT); digitalWrite(PIN_W_HIN, LOW);
  pinMode(PIN_U_LIN, OUTPUT); digitalWrite(PIN_U_LIN, LOW);
  pinMode(PIN_V_LIN, OUTPUT); digitalWrite(PIN_V_LIN, LOW);
  pinMode(PIN_W_LIN, OUTPUT); digitalWrite(PIN_W_LIN, LOW);
  Serial.begin(115200);
  // Core defaults USB to priority 1. Keep motor IRQs (priority 2) above USB
  // so Serial transfers cannot postpone ADC processing or commutation.
  NVIC_SetPriority(OTG_FS_IRQn, 4);
  NVIC_SetPriority(OTG_HS_IRQn, 4);
  const bool clockOK = configureMotorClock();
  const bool timersOK = clockOK && configureTimers();
  configurePhaseAdc();
  configureCurrentMonitor();
  const bool watchdogOK = clockOK && configureWatchdog();
  timerInitOK = timersOK;
  watchdogInitOK = watchdogOK;
  peripheralsReady = timersOK && watchdogOK;
  if (!peripheralsReady) { fault = Fault::CLOCK; state = State::FAULT; }
  // No wait for USB attachment. Opening Serial later and sending HELP works.
  Serial.print(F("sixstepf405 v33-proto1; driver=EG2132; PWM="));
  Serial.print(PWM_FREQUENCY_HZ / 1000UL);
  Serial.println(F("kHz; hardware-preloaded pulses; USB CDC"));
  Serial.println(F("Startup DISARMED; current scan OFF; protection OFF. Send HELLO or HELP."));
}
uint16_t driveTargetPermille() {
  const uint16_t requested = dutyPercent * 10;
  const bool acquiring = state == State::STARTUP ||
      (state == State::CLOSED_LOOP && closedLoopSteps < CLOSED_LOOP_SETTLE_STEPS);
  return acquiring ? min(requested, STARTUP_MAX_PERMILLE) : requested;
}
void updateDriveRamp(uint32_t now) {
  if (state != State::STARTUP && state != State::CLOSED_LOOP) return;
  if (now - lastDutyRampUs < dutyRampTickUs) return;
  // One small increment only: do not jump to catch up after a delayed loop.
  // Keep this same ramp running across the BEMF handover.
  lastDutyRampUs = now;
  const uint16_t target = driveTargetPermille();
  if (effectiveDutyPermille < target) ++effectiveDutyPermille;
  else if (effectiveDutyPermille > target) --effectiveDutyPermille;
}
void loop() {
  IWDG->KR = 0xAAAA;
  const uint32_t key = enterCritical();
  const uint32_t loopNow = motorMicros();
  if (state == State::ARMED_IDLE && appHeartbeatSeen && !appHeartbeatFresh(loopNow)) {
    gatesOff();
    state = State::DISARMED;
  }
  if (motorActive(state) && !appHeartbeatFresh(loopNow)) {
    stopNow(Fault::APP_TIMEOUT);
  }
  if (scopeCapturing && (!motorActive(state) || loopNow - scopeStartedUs >= SCOPE_MAX_CAPTURE_US)) {
    scopeCapturing = false; scopeReady = scopeCount > 0;
  }
  if (motorActive(state)) {
    const uint32_t now = loopNow;
    if (state == State::ALIGN) {
      const uint32_t elapsed = min(now - driveStartedUs, ALIGN_US);
      effectiveDutyPermille = ALIGN_START_PERMILLE +
          (ALIGN_END_PERMILLE - ALIGN_START_PERMILLE) * elapsed / ALIGN_US;
    } else updateDriveRamp(now);
    const uint32_t adcAge = motorMicros() - lastAdcUs;
    if (adcAge > ADC_TIMEOUT_US && timerStage != TimerStage::MAKE) {

      stopNow(Fault::ADC_TIMEOUT);
    }
  }
  const Fault pendingFault = fault;
  const bool announceLock = lockNotice;
  lockNotice = false;
  leaveCritical(key);
  if (scopeLive) {
    if (state != State::CLOSED_LOOP) {
      scopeLive = false; scopeCapturing = false; scopeReady = false;
      scopeSending = false; scopeLiveFrame = false; scopeFrameBegin = false; scopeCount = 0;
    } else if (scopeReady && !scopeSending && !scopeCapturing) {
      scopeSending = true; scopeLiveFrame = true; scopeFrameBegin = true;
      scopeSendIndex = 0; scopeSendStartedUs = motorMicros();
    } else if (!scopeReady && !scopeSending && !scopeCapturing &&
               due(loopNow, scopeLiveNextUs)) {
      const uint32_t scopeKey = enterCritical();
      scopeCount = 0; scopeDecimation = 0; scopeStartedUs = motorMicros();
      scopeCapturing = true;
      leaveCritical(scopeKey);
    }
  }
  // Current acquisition and its failure interlock must keep running even if
  // USB stops draining a telemetry reply.
  pollCurrentMonitor();
  serviceTelemetryReply();
  // Defer ordinary serial commands and phase telemetry while a reply is
  // incomplete. Motor and current supervision above still run every loop.
  if (telemetryReplyLength) return;
  if (pendingFault != Fault::NONE && reportedFault != pendingFault) {
    reportedFault = pendingFault;
    Serial.print(F("FAULT: ")); Serial.print(faultName(pendingFault)); Serial.println(F("; outputs latched OFF"));
    printDiagnosis();
  }
  if (announceLock) Serial.println(F("BEMF LOCKED: timer-driven 30-degree commutation."));
  pollSerial();
  if (scopeSending) {
    serviceScopeRead();
    if (scopeLiveFrame) servicePhaseStream();
    return;
  }
  if (!telemetryReplyLength) servicePhaseStream();
}
