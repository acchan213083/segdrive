/*
MIT License

Copyright © 2020 Namabayashi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <TM1637Display.h> // Library for controlling 4-digit 7-segment display

// ==== Pin Configuration ====
// Define all hardware connections to the Arduino board
#define CLK 4                  // Clock pin for TM1637 display
#define DIO 5                  // Data pin for TM1637 display
#define SWITCH_START 6         // Start button input (active LOW)
#define SWITCH1_PIN 7          // Motor1 control switch (active LOW)
#define SWITCH2_PIN 8          // Piezo sensor input for Motor2 (active LOW pulse)
#define MOTOR1_RELAY_PIN 9     // Relay control pin for Motor1
#define MOTOR2_RELAY_PIN 10    // Relay control pin for Motor2
#define MOTOR2_IN1 11          // Motor2 direction control pin 1
#define MOTOR2_IN2 12          // Motor2 direction control pin 2
#define ELECTROMAGNET_PIN 2    // Electromagnet control pin
#define LED 13                 // LED indicator pin

TM1637Display display(CLK, DIO); // Initialize display object

// ==== Segment Definitions for Custom Characters ====
// Define custom segment patterns for scrolling text on the display
const uint8_t MY_SEG_R = 0b01010000;
const uint8_t MY_SEG_E = 0b01111001;
const uint8_t MY_SEG_A = 0b01110111;
const uint8_t MY_SEG_D = 0b01011110;
const uint8_t MY_SEG_Y = 0b01101110;
const uint8_t MY_SEG_F = 0b01110001;
const uint8_t MY_SEG_I = 0b00000110;
const uint8_t MY_SEG_N = 0b01010100;
const uint8_t MY_SEG_S = 0b01101101;
const uint8_t MY_SEG_H = 0b01110110;
const uint8_t MY_SEG_T = 0b01111000;
const uint8_t MY_SEG_BLANK = 0x00; // Blank segment for spacing

// ==== Timing Constants ====
// Define durations and thresholds for motors and phases
const unsigned long MOTOR1_DURATION = 10000; // Motor1 runs for 10 seconds
const unsigned long MOTOR2_PWM = 255;        // Full speed PWM for Motor2
const unsigned long PIEZO_DEBOUNCE = 80;     // Debounce time for piezo sensor
const int MOTOR2_STOP_TARGET = 5;            // Number of hits to stop Motor2
const int MAX_CYCLES = 10;                  // Maximum number of active cycles
const unsigned long CYCLE_ACTIVE = 30000;    // Active phase duration (30 sec)
const unsigned long CYCLE_INTERVAL = 5000;   // Cool phase duration (5 sec)

// ==== System State Variables ====
// Track current status of motors, sensors, timers, and phases
unsigned long activePhaseStart = 0;          // Time when ACTIVE phase started
bool motor1Active = false;                   // Is Motor1 currently running?
unsigned long motor1Start = 0;               // Time when Motor1 was triggered
bool motor2Running = false;                  // Is Motor2 currently running?
int motor2Hits = 0;                          // Number of piezo hits detected
unsigned long lastPiezo = 0;                 // Last time piezo sensor was triggered
int lastPiezoState = HIGH;                   // Previous piezo sensor state
unsigned long lastTick = 0;                  // Last time countdown ticked
int countdown = 0;                           // Countdown timer in seconds
int cycle = 0;                               // Current cycle number
enum Phase { READY_PHASE, COOL_PHASE, ACTIVE_PHASE }; // Phase enum
Phase phase = READY_PHASE;                   // Initial phase
bool magnetActive = false;                   // Is electromagnet active?
unsigned long magnetStart = 0;               // Time when magnet was activated
bool motor1AutoTriggered = false;            // Has Motor1 auto-triggered this cycle?

// ==== Display Scroll Variables ====
// Used for scrolling "READY" message on display
unsigned long lastReadyScroll = 0;           // Last scroll update time
int readyScrollIndex = -4;                   // Scroll index position
const uint8_t MSG_READY[] = { MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y };

// ==== Start Button Variables ====
// Used for detecting single and double presses
unsigned long lastStartTime = 0;             // Last start button press time
int startCount = 0;                          // Number of presses detected
const unsigned long DOUBLE_PRESS_INTERVAL = 500; // Max interval for double press

// ==== Segment Rotation Function (180° flip) ====
// Rotates segment pattern for upside-down display orientation
uint8_t rotate180(uint8_t seg) {
  uint8_t r = 0;
  if (seg & 0b00000001) r |= 0b00001000;
  if (seg & 0b00000010) r |= 0b00010000;
  if (seg & 0b00000100) r |= 0b00100000;
  if (seg & 0b00001000) r |= 0b00000001;
  if (seg & 0b00010000) r |= 0b00000010;
  if (seg & 0b00100000) r |= 0b00000100;
  if (seg & 0b01000000) r |= 0b01000000;
  return r;
}

// ==== Display Time and Cycle with Colon ====
// Shows countdown and cycle number on display, with colon indicators based on hit count
void showTimeCycleWithHits(int sec, int cyc, int hits, unsigned long now) {
  static bool colonBlinkState = false;
  static unsigned long lastBlinkTime = 0;
  const uint8_t COLON_SEGMENT = 0b10000000;

  // Extract digits from seconds and cycle
  int s1 = (sec / 10) % 10;
  int s0 = sec % 10;
  int c1 = (cyc / 10) % 10;
  int c0 = cyc % 10;

  // Encode digits and rotate for display
  uint8_t d[4] = {
    rotate180(display.encodeDigit(c0)),
    rotate180(display.encodeDigit(c1)),
    rotate180(display.encodeDigit(s0)),
    rotate180(display.encodeDigit(s1))
  };

  // Add colon indicators based on hit count
  switch (hits) {
    case 1:
      d[1] |= COLON_SEGMENT;
      d[2] |= COLON_SEGMENT;
      break;
    case 2:
      d[1] |= COLON_SEGMENT;
      break;
    case 3:
      d[2] |= COLON_SEGMENT;
      break;
    case 4:
      if (now - lastBlinkTime >= 500) {
        colonBlinkState = !colonBlinkState;
        lastBlinkTime = now;
      }
      if (colonBlinkState) d[1] |= COLON_SEGMENT;
      else d[2] |= COLON_SEGMENT;
      break;
  }

  display.setSegments(d); // Update display with final digits
}

// ==== Stop All Motors ====
// Turns off all motors and resets motor-related flags
void stopMotors() {
  motor1Active = false;
  motor2Running = false;
  motor2Hits = 0;
  digitalWrite(MOTOR1_RELAY_PIN, LOW);
  digitalWrite(MOTOR2_RELAY_PIN, LOW);
  analogWrite(MOTOR2_IN1, 0);
  analogWrite(MOTOR2_IN2, 0);
  digitalWrite(LED, LOW);
}

// ==== Motor1 Control ====
// Handles Motor1 timing and phase-based shutdown
void updateMotor1(unsigned long now) {
  if (motor1Active && (now - motor1Start >= MOTOR1_DURATION)) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
//    Serial.println("Motor1 stopped after 10s");
  }
  if (phase != ACTIVE_PHASE && motor1Active) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
    // Serial.println("Motor1 stopped due to phase change");
  }
  if (motor1Active) {
    digitalWrite(MOTOR1_RELAY_PIN, HIGH);
    // Serial.println("Motor1 running...");
  }
}

// ==== Motor2 Drive ====
// Controls Motor2 during the ACTIVE phase.
// Uses PWM to drive Motor2 forward and turns on the relay and LED.
// Stops the motor and turns off indicators when not running.
void driveMotor2() {
  if (motor2Running && phase == ACTIVE_PHASE) {
    analogWrite(MOTOR2_IN1, MOTOR2_PWM);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, HIGH);
    digitalWrite(LED, HIGH);
  } else {
    analogWrite(MOTOR2_IN1, 0);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, LOW);
    digitalWrite(LED, LOW);
  }
}

// ==== Piezo Sensor Check for Motor2 ====
// Detects hits from the piezo sensor during ACTIVE phase.
// Increments hit count and stops Motor2 when target is reached.
void checkMotor2Piezo(unsigned long now) {
  if (phase != ACTIVE_PHASE) {
    lastPiezoState = digitalRead(SWITCH2_PIN);
    return;
  }
  int p = digitalRead(SWITCH2_PIN);
  if (lastPiezoState == HIGH && p == LOW && (now - lastPiezo) > PIEZO_DEBOUNCE) {
    motor2Hits++;
    lastPiezo = now;
    Serial.print("Piezo hit count: ");
    Serial.println(motor2Hits);
    if (motor2Hits >= MOTOR2_STOP_TARGET) motor2Running = false;
  }
  lastPiezoState = p;
}

// ==== Electromagnet Activation ====
// Turns ON the electromagnet and records the start time.
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW); // LOW = ON
  magnetActive = true;
  magnetStart = millis();
}

// ==== Electromagnet Deactivation after 10s ====
// Turns OFF the electromagnet after 10 seconds.
void updateMagnet(unsigned long now) {
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH); // HIGH = OFF
    magnetActive = false;
  }
}

// ==== Scroll "READY" Message ====
// Scrolls the word "READY" across the 4-digit display.
void scrollReadyLoop() {
  unsigned long now = millis();
  if (now - lastReadyScroll >= 200) {
    lastReadyScroll = now;
    const int len = 5;
    uint8_t frame[4];
    for (int i = 0; i < 4; i++) {
      int idx = readyScrollIndex - i;
      frame[i] = (idx >= 0 && idx < len) ? rotate180(MSG_READY[idx]) : MY_SEG_BLANK;
    }
    display.setSegments(frame);
    readyScrollIndex++;
    if (readyScrollIndex > (len + 3)) readyScrollIndex = -4;
  }
}

// ==== Scroll Custom Message ====
// Scrolls any custom message across the display, one frame at a time.
void scrollMessage180(const uint8_t* msg, int len, int delayMs) {
  for (int i = len + 3; i >= 0; i--) {
    uint8_t frame[4] = { 0 };
    for (int j = 0; j < 4; j++) {
      int idx = i - j;
      frame[3 - j] = (idx >= 0 && idx < len) ? rotate180(msg[len - idx - 1]) : MY_SEG_BLANK;
    }
    display.setSegments(frame);
    delay(delayMs);
  }
}

// ==== Reset System to Initial READY State ====
// Resets all counters, stops motors, and returns to READY phase.
void resetSystem() {
  countdown = 0;
  cycle = 0;
  phase = READY_PHASE;
  stopMotors();
  magnetActive = false;
  digitalWrite(ELECTROMAGNET_PIN, HIGH);
  lastTick = millis();
  readyScrollIndex = -4;
  showTimeCycleWithHits(countdown, cycle, motor2Hits, millis());
}

// ==== Arduino Setup ====
// This function runs once when the Arduino is powered on or reset.
// It initializes all pins and sets the system to the READY state.
void setup() {
  Serial.begin(9600); // Start serial communication for debugging

  // Configure input pins
  pinMode(SWITCH_START, INPUT_PULLUP); // Start button (active LOW)
  pinMode(SWITCH1_PIN, INPUT_PULLUP);  // Motor1 control switch (active LOW)
  pinMode(SWITCH2_PIN, INPUT);         // Piezo sensor input (active LOW pulse)

  // Configure output pins
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);   // Relay for Motor1
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);   // Relay for Motor2
  pinMode(MOTOR2_IN1, OUTPUT);         // Motor2 direction control pin 1
  pinMode(MOTOR2_IN2, OUTPUT);         // Motor2 direction control pin 2
  pinMode(ELECTROMAGNET_PIN, OUTPUT);  // Electromagnet control pin
  pinMode(LED, OUTPUT);                // LED indicator

  // Initialize system state
  stopMotors();                        // Ensure all motors are stopped
  digitalWrite(ELECTROMAGNET_PIN, HIGH); // Turn off electromagnet (HIGH = OFF)
  display.setBrightness(7);            // Set display brightness to maximum
  resetSystem();                       // Reset system to READY phase
}

// ==== Main Loop ====
// This function runs continuously after setup().
// It handles button input, phase transitions, motor control, and display updates.
void loop() {
  unsigned long now = millis(); // Get current time in milliseconds

  // --- Debounce for Start Button ---
  // Prevents false triggers due to mechanical noise when pressing the button.
  const unsigned long DEBOUNCE_DELAY = 50;
  static int lastRawStart = HIGH;
  static int stableStart = HIGH;
  static unsigned long lastStartDebounce = 0;
  static bool startHandled = false;

  int rawStart = digitalRead(SWITCH_START);
  if (rawStart != lastRawStart) {
    lastStartDebounce = now;
    startHandled = false;
  }
  lastRawStart = rawStart;

  // Check if button state is stable after debounce delay
  if ((now - lastStartDebounce) > DEBOUNCE_DELAY) {
    if (rawStart != stableStart) {
      stableStart = rawStart;
      if (stableStart == LOW && !startHandled) {
        startHandled = true;

        // --- Detect single or double press ---
        // Double press = reset system
        // Single press = toggle between READY and COOL phase
        if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
        else startCount = 1;
        lastStartTime = now;

        if (startCount >= 2) {
          stopMotors();
          const uint8_t MSG_RESET[] = { MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T };
          scrollMessage180(MSG_RESET, 5, 200);
          resetSystem();
          startCount = 0;
        } else {
          if (phase == READY_PHASE) {
            phase = COOL_PHASE;
            countdown = CYCLE_INTERVAL / 1000;
            lastTick = now;
            stopMotors();
          } else {
            phase = READY_PHASE;
            countdown = 0;
            stopMotors();
            readyScrollIndex = -4;
          }
        }
      }
    }
  }

  // --- Phase Control ---
  switch (phase) {
    case READY_PHASE:
      // Display scrolling "READY" message until user starts
      stopMotors();
      scrollReadyLoop();
      break;

    case COOL_PHASE:
      // Short pause between cycles
      stopMotors();
      motor1AutoTriggered = false; // Reset auto-trigger flag for Motor1
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          // Transition to ACTIVE phase
          phase = ACTIVE_PHASE;
          countdown = CYCLE_ACTIVE / 1000;
          cycle++;
          motor2Running = true;
          motor2Hits = 0;
          activateMagnet(); // Turn on electromagnet
          activePhaseStart = now;
        }
      }
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now);
      break;

    case ACTIVE_PHASE:
      // Main working phase: motors run, piezo counts hits

      // --- Debug output to serial monitor ---
      // Serial.print("Cycle: ");
      // Serial.print(cycle);
      // Serial.print(" | Countdown: ");
      // Serial.print(countdown);
      // Serial.print(" | motor1AutoTriggered: ");
      // Serial.println(motor1AutoTriggered);

      // --- Countdown timer ---
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          motor1AutoTriggered = false; // Reset for next cycle
          if (cycle >= MAX_CYCLES) {
            // All cycles complete: show "FINISH" and reset
            stopMotors();
            const uint8_t MSG_FINISH[] = { MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H };
            scrollMessage180(MSG_FINISH, 6, 300);
            resetSystem();
            break;
          }
          // Move to COOL phase
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000;
          stopMotors();
        }
      }

      // --- Debounce for Motor1 Button ---
      const unsigned long DEBOUNCE_S1 = 50;
      static int lastRawS1 = HIGH;
      static unsigned long lastS1Debounce = 0;

      int rawS1 = digitalRead(SWITCH1_PIN);
      if (rawS1 != lastRawS1) {
        lastS1Debounce = now;
      }
      lastRawS1 = rawS1;

      // Manual trigger for Motor1
      if (digitalRead(SWITCH1_PIN) == LOW) {
        motor1Start = now;
        motor1Active = true;
      }

      // Auto-trigger Motor1 at 20s remaining (once per cycle)
      if (!motor1Active && !motor1AutoTriggered && countdown == 20) {
        motor1Start = now;
        motor1Active = true;
        motor1AutoTriggered = true;
      }

      // --- Update Motor1 status ---
      updateMotor1(now);

      // --- Stop Motor2 if hit target or time is almost up ---
      if (motor2Running && (motor2Hits >= MOTOR2_STOP_TARGET || (now - activePhaseStart >= CYCLE_ACTIVE - 10000))) {
        motor2Running = false;
      }

      // --- Update Motor2 and other components ---
      driveMotor2();
      checkMotor2Piezo(now);
      updateMagnet(now);
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now);
      break;
  }
}
