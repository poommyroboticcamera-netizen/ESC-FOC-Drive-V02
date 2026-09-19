#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Safe, open-loop bring-up firmware for the six EG2132 inputs on header H2.
// Target: Arduino Uno / Nano classic, ATmega328P, 16 MHz.
// This is a bench-test program, not production motor-control firmware.

#if !defined(__AVR_ATmega328P__)
#error "This test firmware requires an ATmega328P Arduino Uno/Nano classic."
#endif

// H2 mapping from SCH_drive_2026-08-24.pdf.
constexpr uint8_t PIN_U_HIN = 9;   // H2-8, OC1A
constexpr uint8_t PIN_V_HIN = 10;  // H2-9, OC1B
constexpr uint8_t PIN_W_HIN = 11;  // H2-10, OC2A
constexpr uint8_t PIN_U_LIN = 4;   // H2-5
constexpr uint8_t PIN_V_LIN = 7;   // H2-6
constexpr uint8_t PIN_W_LIN = 8;   // H2-7

constexpr uint8_t PIN_CURRENT_U = A0;  // H2-2
constexpr uint8_t PIN_CURRENT_V = A1;  // H2-3
constexpr uint8_t PIN_CURRENT_W = A2;  // H2-4

constexpr uint8_t STATE_BLANKING_US = 5;
constexpr uint16_t TEST_STEP_PERIOD_MS = 500;
constexpr uint32_t START_STEP_PERIOD_US = 20000UL;
constexpr uint32_t DEFAULT_TARGET_STEP_PERIOD_US = 5000UL;
constexpr uint32_t MIN_STEP_PERIOD_US = 1000UL;
constexpr uint32_t MAX_STEP_PERIOD_US = 100000UL;
constexpr uint16_t RAMP_DECREMENT_US = 100;

constexpr uint8_t DEFAULT_DUTY_PERCENT = 12;
constexpr uint8_t MAX_DUTY_PERCENT = 60;

// Raw Uno ADC counts relative to the calibrated zero-current voltage.
// U13-U15 have no part number/gain in the schematic, so this cannot yet be
// expressed in amperes. Set to 0 with "trip 0" only for signal-only testing.
constexpr uint16_t DEFAULT_CURRENT_TRIP_COUNTS = 80;
constexpr uint8_t CURRENT_TRIP_CONFIRMATIONS = 3;
constexpr uint32_t CURRENT_SAMPLE_INTERVAL_US = 500UL;

enum class ControllerState : uint8_t {
  DISARMED,
  ARMED_IDLE,
  RUNNING,
  SLOW_TEST,
  FAULT
};

struct CommutationStep {
  uint8_t highPwmPin;
  uint8_t lowOnPin;
};

// Forward six-step sequence: U+V-, U+W-, V+W-, V+U-, W+U-, W+V-.
const CommutationStep COMMUTATION[6] = {
    {PIN_U_HIN, PIN_V_LIN},
    {PIN_U_HIN, PIN_W_LIN},
    {PIN_V_HIN, PIN_W_LIN},
    {PIN_V_HIN, PIN_U_LIN},
    {PIN_W_HIN, PIN_U_LIN},
    {PIN_W_HIN, PIN_V_LIN},
};

ControllerState state = ControllerState::DISARMED;
bool forwardDirection = true;
uint8_t stepIndex = 0;
uint8_t dutyPercent = DEFAULT_DUTY_PERCENT;
uint8_t dutyCounts = (DEFAULT_DUTY_PERCENT * 255UL + 50UL) / 100UL;
uint32_t targetStepPeriodUs = DEFAULT_TARGET_STEP_PERIOD_US;
uint32_t activeStepPeriodUs = START_STEP_PERIOD_US;
uint32_t nextStepAtUs = 0;

uint16_t currentZero[3] = {0, 0, 0};
uint16_t currentRaw[3] = {0, 0, 0};
uint16_t currentTripCounts = DEFAULT_CURRENT_TRIP_COUNTS;
uint8_t currentTripConfirmations = 0;
uint16_t lastFaultPeakCounts = 0;
uint32_t nextCurrentSampleAtUs = 0;

char commandBuffer[48];
uint8_t commandLength = 0;

void disableAllHighPwm() {
  const uint8_t savedSreg = SREG;
  cli();
  TCCR1A &= ~(_BV(COM1A1) | _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0));
  TCCR2A &= ~(_BV(COM2A1) | _BV(COM2A0));
  SREG = savedSreg;

  digitalWrite(PIN_U_HIN, LOW);
  digitalWrite(PIN_V_HIN, LOW);
  digitalWrite(PIN_W_HIN, LOW);
}

void allGatesOff() {
  disableAllHighPwm();
  digitalWrite(PIN_U_LIN, LOW);
  digitalWrite(PIN_V_LIN, LOW);
  digitalWrite(PIN_W_LIN, LOW);
}

void enableHighPwm(uint8_t pin, uint8_t duty) {
  if (duty == 0) {
    return;
  }

  const uint8_t savedSreg = SREG;
  cli();
  if (pin == PIN_U_HIN) {
    OCR1A = duty;
    TCCR1A |= _BV(COM1A1);  // Non-inverting PWM on D9 / OC1A.
  } else if (pin == PIN_V_HIN) {
    OCR1B = duty;
    TCCR1A |= _BV(COM1B1);  // Non-inverting PWM on D10 / OC1B.
  } else if (pin == PIN_W_HIN) {
    OCR2A = duty;
    TCCR2A |= _BV(COM2A1);  // Non-inverting PWM on D11 / OC2A.
  }
  SREG = savedSreg;
}

void configurePwm31kHz() {
  pinMode(PIN_U_HIN, OUTPUT);
  pinMode(PIN_V_HIN, OUTPUT);
  pinMode(PIN_W_HIN, OUTPUT);
  pinMode(PIN_U_LIN, OUTPUT);
  pinMode(PIN_V_LIN, OUTPUT);
  pinMode(PIN_W_LIN, OUTPUT);

  allGatesOff();

  const uint8_t savedSreg = SREG;
  cli();

  // Timer 1: 8-bit phase-correct PWM, TOP=255, prescaler=1.
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS10);
  TCCR1C = 0;
  TCNT1 = 0;
  OCR1A = 0;
  OCR1B = 0;

  // Timer 2: phase-correct PWM, TOP=255, prescaler=1.
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(CS20);
  TCNT2 = 0;
  OCR2A = 0;

  SREG = savedSreg;
  allGatesOff();
}

void applyCommutationStep(uint8_t index) {
  allGatesOff();
  delayMicroseconds(STATE_BLANKING_US);

  const CommutationStep &step = COMMUTATION[index];
  digitalWrite(step.lowOnPin, HIGH);
  delayMicroseconds(STATE_BLANKING_US);
  enableHighPwm(step.highPwmPin, dutyCounts);
}

void prechargeBootstraps() {
  allGatesOff();
  delayMicroseconds(STATE_BLANKING_US);
  digitalWrite(PIN_U_LIN, HIGH);
  digitalWrite(PIN_V_LIN, HIGH);
  digitalWrite(PIN_W_LIN, HIGH);
  delay(3);
  allGatesOff();
  delayMicroseconds(STATE_BLANKING_US);
}

void readCurrents() {
  currentRaw[0] = analogRead(PIN_CURRENT_U);
  currentRaw[1] = analogRead(PIN_CURRENT_V);
  currentRaw[2] = analogRead(PIN_CURRENT_W);
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

uint16_t absoluteDifference(uint16_t a, uint16_t b) {
  return (a >= b) ? (a - b) : (b - a);
}

void latchCurrentFault(uint16_t peakCounts) {
  allGatesOff();
  state = ControllerState::FAULT;
  lastFaultPeakCounts = peakCounts;
  currentTripConfirmations = 0;
  Serial.print(F("FAULT: current-sense delta = "));
  Serial.print(peakCounts);
  Serial.println(F(" ADC counts. Outputs are latched OFF; inspect hardware, then ARM again."));
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
      latchCurrentFault(peakCounts);
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
    nextStepAtUs = micros() + (uint32_t)TEST_STEP_PERIOD_MS * 1000UL;
  }
}

void printState() {
  Serial.print(F("state="));
  switch (state) {
    case ControllerState::DISARMED: Serial.print(F("DISARMED")); break;
    case ControllerState::ARMED_IDLE: Serial.print(F("ARMED_IDLE")); break;
    case ControllerState::RUNNING: Serial.print(F("RUNNING")); break;
    case ControllerState::SLOW_TEST: Serial.print(F("SLOW_TEST")); break;
    case ControllerState::FAULT: Serial.print(F("FAULT")); break;
  }

  readCurrents();
  Serial.print(F(" duty="));
  Serial.print(dutyPercent);
  Serial.print(F("% target_period_us="));
  Serial.print(targetStepPeriodUs);
  Serial.print(F(" direction="));
  Serial.print(forwardDirection ? F("fwd") : F("rev"));
  Serial.print(F(" trip="));
  Serial.print(currentTripCounts);
  Serial.print(F(" raw(U,V,W)="));
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
}

void printHelp() {
  Serial.println(F("Commands (send with Newline):"));
  Serial.println(F("  arm           - gates OFF, calibrate current zero, arm outputs"));
  Serial.println(F("  test          - slow six-step gate/scope test (motor disconnected)"));
  Serial.println(F("  run           - open-loop motor start with a speed ramp"));
  Serial.println(F("  stop          - immediately switch all six inputs LOW and disarm"));
  Serial.println(F("  duty N        - high-side PWM duty, 1..60 percent"));
  Serial.println(F("  period N      - target commutation step period, 1000..100000 us"));
  Serial.println(F("  dir fwd|rev   - direction; accepted only while outputs are idle"));
  Serial.println(F("  trip N        - current trip delta in raw ADC counts; 0 disables"));
  Serial.println(F("  zero          - recalibrate current zero while outputs are idle"));
  Serial.println(F("  status        - print state and current ADC values"));
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
  lastFaultPeakCounts = 0;
  state = ControllerState::ARMED_IDLE;
  Serial.println(F("ARMED: outputs remain OFF. Send TEST or RUN."));
  printState();
}

void startController(bool slowTest) {
  if (state != ControllerState::ARMED_IDLE) {
    Serial.println(F("Rejected: send ARM first."));
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
                     ? F("SLOW TEST started. Motor must be disconnected.")
                     : F("OPEN-LOOP RUN started."));
}

void stopController() {
  allGatesOff();
  state = ControllerState::DISARMED;
  currentTripConfirmations = 0;
  Serial.println(F("STOPPED and DISARMED: all EG2132 inputs LOW."));
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
    stopController();
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
      Serial.println(F("Rejected: duty must be 1..60 percent."));
    } else {
      dutyPercent = (uint8_t)value;
      dutyCounts = (uint8_t)((value * 255L + 50L) / 100L);
      Serial.println(F("Duty updated; it takes effect at the next commutation step."));
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
    if (!ok || value < 0 || value > 500) {
      Serial.println(F("Rejected: trip must be 0..500 ADC counts."));
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

void setup() {
  configurePwm31kHz();
  analogReference(DEFAULT);

  Serial.begin(115200);
  delay(50);
  allGatesOff();
  calibrateCurrentZero();

  Serial.println();
  Serial.println(F("EG2132 / BLDC six-step bench test ready."));
  Serial.println(F("PWM = 31.37 kHz. Power stage starts DISARMED."));
  Serial.println(F("Use a current-limited low-voltage DC bus and keep a physical power cut-off nearby."));
  printHelp();
  printState();
}

void loop() {
  pollSerial();
  monitorCurrent();
  serviceCommutation();
}
