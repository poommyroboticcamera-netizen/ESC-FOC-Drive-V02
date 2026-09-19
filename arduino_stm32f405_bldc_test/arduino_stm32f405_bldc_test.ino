#include <Arduino.h>
#include <HardwareTimer.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Safe, open-loop bring-up firmware for SCH_drive_2026-08-24.pdf.
// Target: STM32F405RGT6 on the controller PCB, using STM32duino.
// This is bench-test firmware. It is not production motor-control firmware.

#if !defined(STM32F405xx)
#error "Select Generic STM32F4 series / Generic F405RGTx in Arduino IDE."
#endif

// Gate-input nets from page 2 of the schematic.
constexpr uint32_t PIN_U_HIN = PA8;   // TIM1_CH1, net UH -> U_HIN
constexpr uint32_t PIN_V_HIN = PA9;   // TIM1_CH2, net VH -> V_HIN
constexpr uint32_t PIN_W_HIN = PA10;  // TIM1_CH3, net WH -> W_HIN
constexpr uint32_t PIN_U_LIN = PB13;  // net UL -> U_LIN
constexpr uint32_t PIN_V_LIN = PB14;  // net VL -> V_LIN
constexpr uint32_t PIN_W_LIN = PB15;  // net WL -> W_LIN

// Current-amplifier outputs from page 4.
constexpr uint32_t PIN_CURRENT_U = PC0;
constexpr uint32_t PIN_CURRENT_V = PC1;
constexpr uint32_t PIN_CURRENT_W = PC2;

// Phase-voltage divider outputs from pages 1 and 2.
constexpr uint32_t PIN_VOLTAGE_U = PA0;
constexpr uint32_t PIN_VOLTAGE_V = PA1;
constexpr uint32_t PIN_VOLTAGE_W = PA2;

// SW3 and SW4 are active-low. During bring-up both are treated as STOP inputs.
constexpr uint32_t PIN_STOP_SW3 = PC4;
constexpr uint32_t PIN_STOP_SW4 = PC5;

constexpr uint32_t PWM_FREQUENCY_HZ = 30000UL;
constexpr uint8_t COMMUTATION_BLANKING_US = 5;
constexpr uint16_t SLOW_TEST_STEP_MS = 800;

constexpr uint32_t START_STEP_PERIOD_US = 30000UL;
constexpr uint32_t DEFAULT_TARGET_STEP_PERIOD_US = 10000UL;
constexpr uint32_t MIN_STEP_PERIOD_US = 1000UL;
constexpr uint32_t MAX_STEP_PERIOD_US = 100000UL;
constexpr uint16_t RAMP_DECREMENT_US = 100;

constexpr uint8_t DEFAULT_DUTY_PERCENT = 8;
constexpr uint8_t MAX_DUTY_PERCENT = 15;

// U13-U15 are INA181A1 (20 V/V) with 1 mOhm shunts. At a 3.3 V, 12-bit ADC
// this gives about 24.8 counts/A. This remains a slow software guard, not
// short-circuit protection.
constexpr float CURRENT_AMP_GAIN = 20.0f;
constexpr float CURRENT_SHUNT_OHMS = 0.001f;
constexpr float ADC_REFERENCE_VOLTS = 3.3f;
constexpr float CURRENT_ADC_COUNTS_PER_AMP =
    CURRENT_AMP_GAIN * CURRENT_SHUNT_OHMS * 4095.0f / ADC_REFERENCE_VOLTS;
constexpr float DEFAULT_CURRENT_TRIP_AMPS = 2.0f;
constexpr uint16_t DEFAULT_CURRENT_TRIP_COUNTS =
    (uint16_t)(DEFAULT_CURRENT_TRIP_AMPS * CURRENT_ADC_COUNTS_PER_AMP + 0.5f);
constexpr uint8_t CURRENT_TRIP_CONFIRMATIONS = 3;
constexpr uint32_t CURRENT_SAMPLE_INTERVAL_US = 250UL;
constexpr uint16_t CURRENT_ZERO_MIN = 1400;
constexpr uint16_t CURRENT_ZERO_MAX = 2700;

enum class ControllerState : uint8_t {
  DISARMED,
  ARMED_IDLE,
  RUNNING,
  SLOW_TEST,
  FAULT
};

struct CommutationStep {
  uint8_t highChannel;
  uint32_t lowPin;
};

// Forward six-step sequence: U+V-, U+W-, V+W-, V+U-, W+U-, W+V-.
const CommutationStep COMMUTATION[6] = {
    {1, PIN_V_LIN},
    {1, PIN_W_LIN},
    {2, PIN_W_LIN},
    {2, PIN_U_LIN},
    {3, PIN_U_LIN},
    {3, PIN_V_LIN},
};

HardwareTimer *pwmTimer = nullptr;
ControllerState state = ControllerState::DISARMED;

bool forwardDirection = true;
uint8_t stepIndex = 0;
uint8_t dutyPercent = DEFAULT_DUTY_PERCENT;
uint32_t targetStepPeriodUs = DEFAULT_TARGET_STEP_PERIOD_US;
uint32_t activeStepPeriodUs = START_STEP_PERIOD_US;
uint32_t nextStepAtUs = 0;

uint16_t currentZero[3] = {0, 0, 0};
uint16_t currentRaw[3] = {0, 0, 0};
uint16_t voltageRaw[3] = {0, 0, 0};
uint16_t currentTripCounts = DEFAULT_CURRENT_TRIP_COUNTS;
uint8_t currentTripConfirmations = 0;
uint32_t nextCurrentSampleAtUs = 0;

char commandBuffer[56];
uint8_t commandLength = 0;

void forceAllGateInputsLow() {
  pinMode(PIN_U_HIN, OUTPUT);
  pinMode(PIN_V_HIN, OUTPUT);
  pinMode(PIN_W_HIN, OUTPUT);
  pinMode(PIN_U_LIN, OUTPUT);
  pinMode(PIN_V_LIN, OUTPUT);
  pinMode(PIN_W_LIN, OUTPUT);

  digitalWrite(PIN_U_HIN, LOW);
  digitalWrite(PIN_V_HIN, LOW);
  digitalWrite(PIN_W_HIN, LOW);
  digitalWrite(PIN_U_LIN, LOW);
  digitalWrite(PIN_V_LIN, LOW);
  digitalWrite(PIN_W_LIN, LOW);
}

void setAllHighDuty(uint8_t u, uint8_t v, uint8_t w) {
  if (pwmTimer == nullptr) {
    return;
  }
  pwmTimer->setCaptureCompare(1, u, PERCENT_COMPARE_FORMAT);
  pwmTimer->setCaptureCompare(2, v, PERCENT_COMPARE_FORMAT);
  pwmTimer->setCaptureCompare(3, w, PERCENT_COMPARE_FORMAT);
  // Force the new compare values into the active registers immediately.
  pwmTimer->refresh();
}

void allGatesOff() {
  setAllHighDuty(0, 0, 0);
  digitalWrite(PIN_U_LIN, LOW);
  digitalWrite(PIN_V_LIN, LOW);
  digitalWrite(PIN_W_LIN, LOW);
}

void configureGatePwm() {
  // Make the pins low as GPIO before handing PA8-PA10 to TIM1.
  forceAllGateInputsLow();

  pwmTimer = new HardwareTimer(TIM1);
  pwmTimer->pause();
  pwmTimer->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PIN_U_HIN);
  pwmTimer->setMode(2, TIMER_OUTPUT_COMPARE_PWM1, PIN_V_HIN);
  pwmTimer->setMode(3, TIMER_OUTPUT_COMPARE_PWM1, PIN_W_HIN);
  pwmTimer->setOverflow(PWM_FREQUENCY_HZ, HERTZ_FORMAT);
  pwmTimer->setCaptureCompare(1, 0, PERCENT_COMPARE_FORMAT);
  pwmTimer->setCaptureCompare(2, 0, PERCENT_COMPARE_FORMAT);
  pwmTimer->setCaptureCompare(3, 0, PERCENT_COMPARE_FORMAT);
  pwmTimer->resume();
  allGatesOff();
}

void enableSelectedHighPwm(uint8_t channel) {
  if (channel == 1) {
    setAllHighDuty(dutyPercent, 0, 0);
  } else if (channel == 2) {
    setAllHighDuty(0, dutyPercent, 0);
  } else {
    setAllHighDuty(0, 0, dutyPercent);
  }
}

void applyCommutationStep(uint8_t index) {
  // Break-before-make: PWM off, all low sides off, blank, then apply the next state.
  allGatesOff();
  delayMicroseconds(COMMUTATION_BLANKING_US);

  const CommutationStep &step = COMMUTATION[index];
  digitalWrite(step.lowPin, HIGH);
  delayMicroseconds(COMMUTATION_BLANKING_US);
  enableSelectedHighPwm(step.highChannel);
}

void prechargeBootstraps() {
  // At standstill this briefly grounds all three phases so C31-C33 can charge.
  allGatesOff();
  delayMicroseconds(COMMUTATION_BLANKING_US);
  digitalWrite(PIN_U_LIN, HIGH);
  digitalWrite(PIN_V_LIN, HIGH);
  digitalWrite(PIN_W_LIN, HIGH);
  delay(3);
  allGatesOff();
  delayMicroseconds(COMMUTATION_BLANKING_US);
}

void readCurrents() {
  currentRaw[0] = analogRead(PIN_CURRENT_U);
  currentRaw[1] = analogRead(PIN_CURRENT_V);
  currentRaw[2] = analogRead(PIN_CURRENT_W);
}

void readVoltages() {
  voltageRaw[0] = analogRead(PIN_VOLTAGE_U);
  voltageRaw[1] = analogRead(PIN_VOLTAGE_V);
  voltageRaw[2] = analogRead(PIN_VOLTAGE_W);
}

void calibrateCurrentZero() {
  allGatesOff();
  delay(20);

  uint32_t sums[3] = {0, 0, 0};
  constexpr uint16_t sampleCount = 128;
  for (uint16_t i = 0; i < sampleCount; ++i) {
    sums[0] += analogRead(PIN_CURRENT_U);
    sums[1] += analogRead(PIN_CURRENT_V);
    sums[2] += analogRead(PIN_CURRENT_W);
    delayMicroseconds(100);
  }

  for (uint8_t i = 0; i < 3; ++i) {
    currentZero[i] = sums[i] / sampleCount;
  }
  readCurrents();
}

bool currentZeroLooksValid() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (currentZero[i] < CURRENT_ZERO_MIN || currentZero[i] > CURRENT_ZERO_MAX) {
      return false;
    }
  }
  return true;
}

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return (a >= b) ? (a - b) : (b - a);
}

void latchFault(const __FlashStringHelper *message) {
  allGatesOff();
  state = ControllerState::FAULT;
  currentTripConfirmations = 0;
  Serial.print(F("FAULT: "));
  Serial.println(message);
  Serial.println(F("Outputs are latched OFF. Inspect the hardware, then send ARM again."));
}

void monitorCurrent() {
  if (state != ControllerState::RUNNING && state != ControllerState::SLOW_TEST) {
    return;
  }
  if (currentTripCounts == 0) {
    return;
  }

  const uint32_t now = micros();
  if ((int32_t)(now - nextCurrentSampleAtUs) < 0) {
    return;
  }
  nextCurrentSampleAtUs = now + CURRENT_SAMPLE_INTERVAL_US;

  readCurrents();
  uint16_t peakCounts = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    const uint16_t delta = absoluteDifference(currentRaw[i], currentZero[i]);
    if (delta > peakCounts) {
      peakCounts = delta;
    }
  }

  if (peakCounts >= currentTripCounts) {
    ++currentTripConfirmations;
    if (currentTripConfirmations >= CURRENT_TRIP_CONFIRMATIONS) {
      allGatesOff();
      state = ControllerState::FAULT;
      currentTripConfirmations = 0;
      Serial.print(F("FAULT: current delta reached "));
      Serial.print(peakCounts);
      Serial.println(F(" ADC counts. Outputs are latched OFF."));
    }
  } else {
    currentTripConfirmations = 0;
  }
}

void serviceCommutation() {
  if (state != ControllerState::RUNNING && state != ControllerState::SLOW_TEST) {
    return;
  }

  const uint32_t now = micros();
  if ((int32_t)(now - nextStepAtUs) < 0) {
    return;
  }

  applyCommutationStep(stepIndex);
  if (forwardDirection) {
    stepIndex = (stepIndex + 1) % 6;
  } else {
    stepIndex = (stepIndex + 5) % 6;
  }

  if (state == ControllerState::RUNNING) {
    if (activeStepPeriodUs > targetStepPeriodUs) {
      const uint32_t nextPeriod = activeStepPeriodUs - RAMP_DECREMENT_US;
      activeStepPeriodUs = (nextPeriod < targetStepPeriodUs) ? targetStepPeriodUs : nextPeriod;
    }
    nextStepAtUs = micros() + activeStepPeriodUs;
  } else {
    nextStepAtUs = micros() + (uint32_t)SLOW_TEST_STEP_MS * 1000UL;
  }
}

const __FlashStringHelper *stateName() {
  switch (state) {
    case ControllerState::DISARMED: return F("DISARMED");
    case ControllerState::ARMED_IDLE: return F("ARMED_IDLE");
    case ControllerState::RUNNING: return F("RUNNING");
    case ControllerState::SLOW_TEST: return F("SLOW_TEST");
    case ControllerState::FAULT: return F("FAULT");
  }
  return F("UNKNOWN");
}

void printState() {
  readCurrents();
  readVoltages();

  Serial.print(F("state="));
  Serial.print(stateName());
  Serial.print(F(" duty="));
  Serial.print(dutyPercent);
  Serial.print(F("% target_step_us="));
  Serial.print(targetStepPeriodUs);
  Serial.print(F(" direction="));
  Serial.print(forwardDirection ? F("fwd") : F("rev"));
  Serial.print(F(" trip="));
  Serial.print(currentTripCounts);
  Serial.print(F(" counts (~"));
  Serial.print((float)currentTripCounts / CURRENT_ADC_COUNTS_PER_AMP, 2);
  Serial.println(F(" A)"));

  Serial.print(F("current raw(U,V,W)="));
  Serial.print(currentRaw[0]);
  Serial.print(',');
  Serial.print(currentRaw[1]);
  Serial.print(',');
  Serial.print(currentRaw[2]);
  Serial.print(F(" zero="));
  Serial.print(currentZero[0]);
  Serial.print(',');
  Serial.print(currentZero[1]);
  Serial.print(',');
  Serial.println(currentZero[2]);

  Serial.print(F("phase voltage ADC(U,V,W)="));
  Serial.print(voltageRaw[0]);
  Serial.print(',');
  Serial.print(voltageRaw[1]);
  Serial.print(',');
  Serial.println(voltageRaw[2]);
}

void printHelp() {
  Serial.println(F("Commands (send with Newline):"));
  Serial.println(F("  arm           gates OFF, calibrate current zero, then arm"));
  Serial.println(F("  test          slow six-step input/scope test; disconnect motor"));
  Serial.println(F("  run           open-loop motor start with speed ramp"));
  Serial.println(F("  stop          immediately switch all six gate inputs LOW"));
  Serial.println(F("  duty N        PWM duty, 1..15 percent"));
  Serial.println(F("  period N      target step period, 1000..100000 us"));
  Serial.println(F("  dir fwd|rev   direction; change only while stopped"));
  Serial.println(F("  trip N        current delta trip, 1..1800 ADC counts"));
  Serial.println(F("  trip 0        disable software trip for logic-only testing"));
  Serial.println(F("  zero          recalibrate current zero while stopped"));
  Serial.println(F("  status        print ADC readings and controller state"));
  Serial.println(F("  help"));
}

bool outputsAreIdle() {
  return state == ControllerState::DISARMED ||
         state == ControllerState::ARMED_IDLE ||
         state == ControllerState::FAULT;
}

void armController() {
  allGatesOff();
  state = ControllerState::DISARMED;
  calibrateCurrentZero();
  currentTripConfirmations = 0;

  if (currentTripCounts != 0 && !currentZeroLooksValid()) {
    latchFault(F("current zero is outside the expected 1.65 V region"));
    Serial.println(F("Check U13-U15. For an unpowered logic-only test, use TRIP 0 then ARM."));
    printState();
    return;
  }

  state = ControllerState::ARMED_IDLE;
  Serial.println(F("ARMED: outputs remain OFF. Send TEST or RUN."));
  printState();
}

void startController(bool slowTest) {
  if (state != ControllerState::ARMED_IDLE) {
    Serial.println(F("Rejected: send ARM first."));
    return;
  }
  if (digitalRead(PIN_STOP_SW3) == LOW || digitalRead(PIN_STOP_SW4) == LOW) {
    Serial.println(F("Rejected: release SW3 and SW4 first."));
    return;
  }

  prechargeBootstraps();
  stepIndex = forwardDirection ? 0 : 5;
  activeStepPeriodUs = (targetStepPeriodUs > START_STEP_PERIOD_US)
                           ? targetStepPeriodUs
                           : START_STEP_PERIOD_US;
  currentTripConfirmations = 0;
  nextCurrentSampleAtUs = micros();
  state = slowTest ? ControllerState::SLOW_TEST : ControllerState::RUNNING;
  nextStepAtUs = micros();
  Serial.println(slowTest
                     ? F("SLOW TEST started. Disconnect the motor.")
                     : F("OPEN-LOOP RUN started."));
}

void stopController(const __FlashStringHelper *reason) {
  allGatesOff();
  state = ControllerState::DISARMED;
  currentTripConfirmations = 0;
  Serial.print(F("STOPPED: "));
  Serial.println(reason);
}

long parseLongArgument(char *argument, bool &ok) {
  ok = false;
  if (argument == nullptr || *argument == '\0') {
    return 0;
  }

  char *end = nullptr;
  const long value = strtol(argument, &end, 10);
  if (*end == '\0') {
    ok = true;
  }
  return value;
}

void handleCommand(char *line) {
  for (char *p = line; *p != '\0'; ++p) {
    *p = (char)tolower((unsigned char)*p);
  }

  char *command = strtok(line, " ");
  char *argument = strtok(nullptr, " ");
  if (command == nullptr) {
    return;
  }

  if (strcmp(command, "arm") == 0) {
    armController();
  } else if (strcmp(command, "test") == 0) {
    startController(true);
  } else if (strcmp(command, "run") == 0) {
    startController(false);
  } else if (strcmp(command, "stop") == 0) {
    stopController(F("serial command; all gate inputs are LOW"));
  } else if (strcmp(command, "status") == 0) {
    printState();
  } else if (strcmp(command, "help") == 0) {
    printHelp();
  } else if (strcmp(command, "zero") == 0) {
    if (!outputsAreIdle()) {
      Serial.println(F("Rejected: STOP before zero calibration."));
    } else {
      calibrateCurrentZero();
      Serial.println(F("Current zero recalibrated."));
      printState();
    }
  } else if (strcmp(command, "duty") == 0) {
    bool ok = false;
    const long value = parseLongArgument(argument, ok);
    if (!ok || value < 1 || value > MAX_DUTY_PERCENT) {
      Serial.println(F("Rejected: duty must be 1..15 percent."));
    } else {
      dutyPercent = (uint8_t)value;
      Serial.println(F("Duty updated; it takes effect on the next step."));
    }
  } else if (strcmp(command, "period") == 0) {
    bool ok = false;
    const long value = parseLongArgument(argument, ok);
    if (!ok || value < (long)MIN_STEP_PERIOD_US || value > (long)MAX_STEP_PERIOD_US) {
      Serial.println(F("Rejected: period must be 1000..100000 us."));
    } else {
      targetStepPeriodUs = (uint32_t)value;
      Serial.println(F("Target commutation period updated."));
    }
  } else if (strcmp(command, "trip") == 0) {
    bool ok = false;
    const long value = parseLongArgument(argument, ok);
    if (!ok || value < 0 || value > 1800) {
      Serial.println(F("Rejected: trip must be 0..1800 ADC counts."));
    } else {
      currentTripCounts = (uint16_t)value;
      Serial.println(value == 0
                         ? F("WARNING: software current trip disabled.")
                         : F("Current trip threshold updated."));
    }
  } else if (strcmp(command, "dir") == 0) {
    if (!outputsAreIdle()) {
      Serial.println(F("Rejected: STOP before changing direction."));
    } else if (argument != nullptr && strcmp(argument, "fwd") == 0) {
      forwardDirection = true;
      Serial.println(F("Direction set to forward."));
    } else if (argument != nullptr && strcmp(argument, "rev") == 0) {
      forwardDirection = false;
      Serial.println(F("Direction set to reverse."));
    } else {
      Serial.println(F("Rejected: use 'dir fwd' or 'dir rev'."));
    }
  } else {
    Serial.println(F("Unknown command. Send HELP."));
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      commandBuffer[commandLength] = '\0';
      handleCommand(commandBuffer);
      commandLength = 0;
      continue;
    }
    if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = c;
    } else {
      commandLength = 0;
      Serial.println(F("Command too long; buffer cleared."));
    }
  }
}

void monitorStopButtons() {
  if ((state == ControllerState::RUNNING || state == ControllerState::SLOW_TEST ||
       state == ControllerState::ARMED_IDLE) &&
      (digitalRead(PIN_STOP_SW3) == LOW || digitalRead(PIN_STOP_SW4) == LOW)) {
    stopController(F("SW3/SW4 pressed; all gate inputs are LOW"));
  }
}

void setup() {
  // This is the earliest firmware-controlled point. External pull-downs are still
  // required because MCU pins are high-impedance during reset and programming.
  forceAllGateInputsLow();
  pinMode(PIN_STOP_SW3, INPUT_PULLUP);
  pinMode(PIN_STOP_SW4, INPUT_PULLUP);
  analogReadResolution(12);
  configureGatePwm();

  Serial.begin(115200);
  delay(250);
  allGatesOff();
  calibrateCurrentZero();

  Serial.println();
  Serial.println(F("STM32F405 / 3-phase gate-input bench test ready."));
  Serial.println(F("TIM1 PWM = 30 kHz. Startup state = DISARMED."));
  Serial.println(F("Use a current-limited supply and a physical DC-bus cut-off."));
  printHelp();
  printState();
}

void loop() {
  monitorStopButtons();
  pollSerial();
  monitorCurrent();
  serviceCommutation();
}
