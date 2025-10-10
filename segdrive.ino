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
#define CLK 4                  // Clock pin for TM1637 display
#define DIO 5                  // Data pin for TM1637 display
#define SWITCH_START 6         // Start button input
#define SWITCH1_PIN 7          // Motor1 control switch
#define SWITCH2_PIN 8          // Piezo sensor input for Motor2
#define MOTOR1_RELAY_PIN 9     // Relay control pin for Motor1
#define MOTOR2_RELAY_PIN 10    // Relay control pin for Motor2
#define MOTOR2_IN1 11          // Motor2 direction control pin 1
#define MOTOR2_IN2 12          // Motor2 direction control pin 2
#define ELECTROMAGNET_PIN 2    // Electromagnet control pin
#define LED 13                 // LED indicator pin

TM1637Display display(CLK, DIO); // Initialize display object

// ==== Segment Definitions for Custom Characters ====
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
const uint8_t MY_SEG_BLANK = 0x00; // Blank segment

// ==== Timing Constants ====
const unsigned long MOTOR1_DURATION = 10000; // Motor1 runs for 10 seconds
const unsigned long MOTOR2_PWM = 255;        // Full speed PWM for Motor2
const unsigned long PIEZO_DEBOUNCE = 100;    // Debounce time for piezo sensor
const int MOTOR2_STOP_TARGET = 5;            // Number of hits to stop Motor2
const int MAX_CYCLES = 10;                   // Maximum number of active cycles
const unsigned long CYCLE_ACTIVE = 30000;    // Active phase duration (30 sec)
const unsigned long CYCLE_INTERVAL = 5000;   // Cool phase duration (5 sec)

// ==== System State Variables ====
unsigned long activePhaseStart = 0; 
bool motor1Active = false;
unsigned long motor1Start = 0;
bool motor2Running = false;
int motor2Hits = 0;
unsigned long lastPiezo = 0;
int lastPiezoState = HIGH;
unsigned long lastTick = 0;
int countdown = 0;
int cycle = 0;
enum Phase { READY_PHASE, COOL_PHASE, ACTIVE_PHASE } phase = READY_PHASE;
bool magnetActive = false;
unsigned long magnetStart = 0;

// ==== Display Scroll Variables ====
unsigned long lastReadyScroll = 0;
int readyScrollIndex = -4;
const uint8_t MSG_READY[] = { MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y };

// ==== Start Button Variables ====
unsigned long lastStartTime = 0;
int startCount = 0;
const unsigned long DOUBLE_PRESS_INTERVAL = 500; // Double press detection window

// ==== Segment Rotation Function (180° flip) ====
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

// ==== Display Time and Cycle Count (rotated) ====
void showTimeCycle180(int sec, int cyc) {
  int s1 = (sec / 10) % 10;
  int s0 = sec % 10;
  int c1 = (cyc / 10) % 10;
  int c0 = cyc % 10;
  uint8_t d[4] = {
    rotate180(display.encodeDigit(c0)),
    rotate180(display.encodeDigit(c1)),
    rotate180(display.encodeDigit(s0)),
    rotate180(display.encodeDigit(s1))
  };
  display.setSegments(d);
}

// ==== Stop All Motors ====
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
void updateMotor1(unsigned long now) {
  if (motor1Active && (now - motor1Start >= MOTOR1_DURATION)) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
  }
  if (phase != ACTIVE_PHASE && motor1Active) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
  }
  if (motor1Active) {
    digitalWrite(MOTOR1_RELAY_PIN, HIGH);
  }
}

// ==== Motor2 Drive ====
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
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW); // LOW = ON
  magnetActive = true;
  magnetStart = millis();
}

// ==== Electromagnet Deactivation after 10s ====
void updateMagnet(unsigned long now) {
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH); // HIGH = OFF
    magnetActive = false;
  }
}

// ==== Scroll "READY" Message ====
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
void resetSystem() {
  countdown = 0;
  cycle = 0;
  phase = READY_PHASE;
  stopMotors();
  magnetActive = false;
  digitalWrite(ELECTROMAGNET_PIN, HIGH);
  lastTick = millis();
  readyScrollIndex = -4;
  showTimeCycle180(countdown, cycle);
}

// ==== Arduino Setup ===
void setup() {
  Serial.begin(9600); // Initialize serial communication for debugging and monitoring

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
void loop() {
  unsigned long now = millis(); // Current time in milliseconds

  // --- Handle Start Button Press ---
  static int lastStart = HIGH;
  int start = digitalRead(SWITCH_START);
  if (lastStart == HIGH && start == LOW) {
    // Detect double press within interval
    if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
    else startCount = 1;
    lastStartTime = now;

    if (startCount >= 2) {
      // Double press: reset system
      stopMotors();
      const uint8_t MSG_RESET[] = { MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T };
      scrollMessage180(MSG_RESET, 5, 200);
      resetSystem();
      startCount = 0;
    } else {
      // Single press: toggle between READY and COOL phase
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
  lastStart = start;

  // --- Phase Control ---
  switch (phase) {
    case READY_PHASE:
      // Display scrolling "READY" message
      stopMotors();
      scrollReadyLoop();
      break;

    case COOL_PHASE:
      // Countdown before entering ACTIVE phase
      stopMotors();
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
          activateMagnet();
          activePhaseStart = now;

          // If Motor1 switch is pressed, start Motor1 immediately
          if (digitalRead(SWITCH1_PIN) == LOW) {
            motor1Active = true;
            motor1Start = now;
            digitalWrite(MOTOR1_RELAY_PIN, HIGH);
          }
        }
      }
      showTimeCycle180(countdown, cycle);
      break;

    case ACTIVE_PHASE:
      // Main active phase: motors run, piezo counts hits
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          if (cycle >= MAX_CYCLES) {
            // All cycles completed: show "FINISH" and reset
            stopMotors();
            const uint8_t MSG_FINISH[] = { MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H };
            scrollMessage180(MSG_FINISH, 6, 300);
            resetSystem();
            break;
          }
          // Transition to COOL phase
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000;
          stopMotors();
        }
      }

      // --- Motor1 Switch Handling ---
      static int lastS1 = HIGH;
      int s1 = digitalRead(SWITCH1_PIN);
      if (s1 != lastS1) {
        lastS1 = s1;
      }

      if (phase == ACTIVE_PHASE && s1 == LOW) {
        // Restart Motor1 timer on press
        motor1Start = now;
        if (!motor1Active) {
          motor1Active = true;
        }
        digitalWrite(MOTOR1_RELAY_PIN, HIGH);
      }

      // --- Update Motors and Magnet ---
      updateMotor1(now);
      if (motor2Running && (motor2Hits >= MOTOR2_STOP_TARGET || (now - activePhaseStart >= CYCLE_ACTIVE - 10000))) {
        motor2Running = false;
       }
      driveMotor2();
      checkMotor2Piezo(now);     // Piezo hit detection and count
      updateMagnet(now);         // Turn off magnet after 10s
      showTimeCycle180(countdown, cycle); // Update display
      break;
  }
}