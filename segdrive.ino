/*
MIT License

Copyright © 2025 Namabayashi

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

#include <TM1637Display.h> // Include the library for controlling the TM1637 4-digit 7-segment LED display

// ==== Pin Configuration ====
// Define all hardware connections to the Arduino board
#define CLK 4                  // TM1637 display clock pin
#define DIO 5                  // TM1637 display data pin
#define SWITCH_START 6         // Start button input pin (active LOW when pressed)
#define SWITCH1_PIN 7          // Motor1 control switch input pin (active LOW when pressed)
#define SWITCH2_PIN 8          // Piezo sensor input pin for Motor2 hit detection (active LOW pulse)
#define MOTOR1_RELAY_PIN 9     // Output pin to control Motor1 relay (HIGH = ON)
#define MOTOR2_RELAY_PIN 10    // Output pin to control Motor2 relay (HIGH = ON)
#define MOTOR2_IN1 11          // Motor2 direction control pin 1 (PWM capable)
#define MOTOR2_IN2 12          // Motor2 direction control pin 2 (PWM capable)
#define ELECTROMAGNET_PIN 2    // Output pin to control electromagnet (LOW = ON)
#define LED 13                 // Output pin for LED indicator (HIGH = ON)

TM1637Display display(CLK, DIO); // Create a display object using CLK and DIO pins

// ==== Segment Definitions for Custom Characters ====
// Define custom segment bitmaps for scrolling text on the display
const uint8_t MY_SEG_R = 0b01010000; // Segment pattern for 'R'
const uint8_t MY_SEG_E = 0b01111001; // Segment pattern for 'E'
const uint8_t MY_SEG_A = 0b01110111; // Segment pattern for 'A'
const uint8_t MY_SEG_D = 0b01011110; // Segment pattern for 'D'
const uint8_t MY_SEG_Y = 0b01101110; // Segment pattern for 'Y'
const uint8_t MY_SEG_F = 0b01110001; // Segment pattern for 'F'
const uint8_t MY_SEG_I = 0b00000110; // Segment pattern for 'I'
const uint8_t MY_SEG_N = 0b01010100; // Segment pattern for 'N'
const uint8_t MY_SEG_S = 0b01101101; // Segment pattern for 'S'
const uint8_t MY_SEG_H = 0b01110110; // Segment pattern for 'H'
const uint8_t MY_SEG_T = 0b01111000; // Segment pattern for 'T'
const uint8_t MY_SEG_BLANK = 0x00;   // Blank segment for spacing

// ==== Timing Constants ====
// Define durations and thresholds for motors and phase transitions
const unsigned long MOTOR1_DURATION = 10000; // Motor1 runs for 10 seconds per activation
const unsigned long MOTOR2_PWM = 255;        // Motor2 runs at full PWM speed (255)
const unsigned long PIEZO_DEBOUNCE = 80;     // Debounce time for piezo sensor input (in milliseconds)
const int MOTOR2_STOP_TARGET = 5;            // Motor2 stops after detecting 5 piezo hits
const int MAX_CYCLES = 10;                   // System runs for a maximum of 10 cycles before resetting
const unsigned long CYCLE_ACTIVE = 30000;    // ACTIVE phase duration: 30 seconds
const unsigned long CYCLE_INTERVAL = 5000;   // COOL phase duration: 5 seconds

// ==== System State Variables ====
// Variables to track current status of motors, sensors, timers, and phases
unsigned long activePhaseStart = 0;          // Timestamp when ACTIVE phase started
bool motor1Active = false;                   // Flag indicating if Motor1 is currently running
unsigned long motor1Start = 0;               // Timestamp when Motor1 was last triggered
bool motor2Running = false;                  // Flag indicating if Motor2 is currently running
int motor2Hits = 0;                          // Counter for piezo sensor hits during ACTIVE phase
unsigned long lastPiezo = 0;                 // Timestamp of last piezo sensor activation
int lastPiezoState = HIGH;                   // Previous state of piezo sensor input
unsigned long lastTick = 0;                  // Timestamp of last countdown tick
int countdown = 0;                           // Countdown timer in seconds
int cycle = 0;                               // Current cycle number (0 to MAX_CYCLES)
enum Phase { READY_PHASE, COOL_PHASE, ACTIVE_PHASE }; // Enum to represent system phases
Phase phase = READY_PHASE;                   // Initial phase set to READY
bool magnetActive = false;                   // Flag indicating if electromagnet is currently active
unsigned long magnetStart = 0;               // Timestamp when electromagnet was activated
bool motor1AutoTriggered = false;            // Flag to prevent multiple auto-triggers of Motor1 per cycle

// ==== Display Scroll Variables ====
// Variables used for scrolling "READY" message on the display
unsigned long lastReadyScroll = 0;           // Timestamp of last scroll update
int readyScrollIndex = -4;                   // Scroll index position (starts off-screen)
const uint8_t MSG_READY[] = { MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y }; // Segment array for "READY"

// ==== Start Button Variables ====
// Variables used to detect single and double presses of the start button
unsigned long lastStartTime = 0;             // Timestamp of last start button press
int startCount = 0;                          // Number of presses detected within interval
const unsigned long DOUBLE_PRESS_INTERVAL = 500; // Max interval (ms) between presses to count as double press

// ==== Segment Rotation Function (180° flip) ====
// Rotates a segment pattern to flip the digit upside down for display orientation
uint8_t rotate180(uint8_t seg) {
  uint8_t r = 0;
  if (seg & 0b00000001) r |= 0b00001000;     // Map bottom segment to top-left
  if (seg & 0b00000010) r |= 0b00010000;     // Map bottom-left to top-right
  if (seg & 0b00000100) r |= 0b00100000;     // Map bottom-right to middle
  if (seg & 0b00001000) r |= 0b00000001;     // Map top-left to bottom
  if (seg & 0b00010000) r |= 0b00000010;     // Map top-right to bottom-left
  if (seg & 0b00100000) r |= 0b00000100;     // Map middle to bottom-right
  if (seg & 0b01000000) r |= 0b01000000;     // Keep center segment unchanged
  return r;                                  // Return rotated segment pattern
}

// ==== Display Time and Cycle with Colon ====
// Displays countdown and cycle number on the 7-segment display
// Adds colon indicators based on piezo hit count for visual feedback
void showTimeCycleWithHits(int sec, int cyc, int hits, unsigned long now) {
  static bool colonBlinkState = false;       // State for blinking colon animation
  static unsigned long lastBlinkTime = 0;    // Timestamp of last colon blink
  const uint8_t COLON_SEGMENT = 0b10000000;  // Bitmask to enable colon segment

  // Extract digits from seconds and cycle values
  int s1 = (sec / 10) % 10;                  // Tens digit of seconds
  int s0 = sec % 10;                         // Ones digit of seconds
  int c1 = (cyc / 10) % 10;                  // Tens digit of cycle
  int c0 = cyc % 10;                         // Ones digit of cycle

  // Encode digits and rotate them for upside-down display
  uint8_t d[4] = {
    rotate180(display.encodeDigit(c0)),      // Rightmost digit: cycle ones
    rotate180(display.encodeDigit(c1)),      // Next digit: cycle tens
    rotate180(display.encodeDigit(s0)),      // Next digit: seconds ones
    rotate180(display.encodeDigit(s1))       // Leftmost digit: seconds tens
  };

  // Add colon indicators based on hit count
  switch (hits) {
    case 1:
      // If 1 hit detected, light up both colon positions (cycle and time)
      d[1] |= COLON_SEGMENT; // Add colon to cycle digit
      d[2] |= COLON_SEGMENT; // Add colon to time digit
      break;
    case 2:
      // If 2 hits detected, light up colon between cycle digits only
      d[1] |= COLON_SEGMENT;
      break;
    case 3:
      // If 3 hits detected, light up colon between time digits only
      d[2] |= COLON_SEGMENT;
      break;
    case 4:
      // If 4 hits detected, blink colon between cycle and time digits alternately
      if (now - lastBlinkTime >= 500) { // Check if 500ms has passed since last blink
        colonBlinkState = !colonBlinkState; // Toggle blink state
        lastBlinkTime = now; // Update last blink timestamp
      }
      if (colonBlinkState)
        d[1] |= COLON_SEGMENT; // Show colon on cycle digit
      else
        d[2] |= COLON_SEGMENT; // Show colon on time digit
      break;
  }

  display.setSegments(d); // Send final digit array to the display

// ==== Stop All Motors ====
// Turns off all motors and resets motor-related flags
void stopMotors() {
  motor1Active = false; // Reset Motor1 active flag
  motor2Running = false; // Reset Motor2 running flag
  motor2Hits = 0; // Reset piezo hit counter
  digitalWrite(MOTOR1_RELAY_PIN, LOW); // Turn off Motor1 relay
  digitalWrite(MOTOR2_RELAY_PIN, LOW); // Turn off Motor2 relay
  analogWrite(MOTOR2_IN1, 0); // Stop Motor2 PWM output on IN1
  analogWrite(MOTOR2_IN2, 0); // Stop Motor2 PWM output on IN2
  digitalWrite(LED, LOW); // Turn off LED indicator
}

// ==== Motor1 Control ====
// Handles Motor1 timing and phase-based shutdown
void updateMotor1(unsigned long now) {
  // Check if Motor1 has been running for longer than its duration
  if (motor1Active && (now - motor1Start >= MOTOR1_DURATION)) {
    motor1Active = false; // Stop Motor1
    digitalWrite(MOTOR1_RELAY_PIN, LOW); // Turn off Motor1 relay
    // Serial.println("Motor1 stopped after 10s"); // Optional debug output
  }

  // If phase has changed and Motor1 is still running, stop it immediately
  if (phase != ACTIVE_PHASE && motor1Active) {
    motor1Active = false; // Stop Motor1
    digitalWrite(MOTOR1_RELAY_PIN, LOW); // Turn off Motor1 relay
    // Serial.println("Motor1 stopped due to phase change"); // Optional debug output
  }

  // If Motor1 is active, keep the relay ON
  if (motor1Active) {
    digitalWrite(MOTOR1_RELAY_PIN, HIGH); // Keep Motor1 relay ON
    // Serial.println("Motor1 running..."); // Optional debug output
  }
}

// ==== Motor2 Drive ====
// Controls Motor2 during the ACTIVE phase.
// Uses PWM to drive Motor2 forward and turns on the relay and LED.
// Stops the motor and turns off indicators when not running.
void driveMotor2() {
  // If Motor2 is running and phase is ACTIVE, drive it forward
  if (motor2Running && phase == ACTIVE_PHASE) {
    analogWrite(MOTOR2_IN1, MOTOR2_PWM); // Set IN1 to full PWM speed
    analogWrite(MOTOR2_IN2, 0); // Set IN2 to 0 for forward direction
    digitalWrite(MOTOR2_RELAY_PIN, HIGH); // Turn ON Motor2 relay
    digitalWrite(LED, HIGH); // Turn ON LED indicator
  } else {
    // If Motor2 is not running or phase is not ACTIVE, stop it
    analogWrite(MOTOR2_IN1, 0); // Stop PWM on IN1
    analogWrite(MOTOR2_IN2, 0); // Stop PWM on IN2
    digitalWrite(MOTOR2_RELAY_PIN, LOW); // Turn OFF Motor2 relay
    digitalWrite(LED, LOW); // Turn OFF LED indicator
  }
}

// ==== Piezo Sensor Check for Motor2 ====
// Detects hits from the piezo sensor during ACTIVE phase.
// Increments hit count and stops Motor2 when target is reached.
void checkMotor2Piezo(unsigned long now) {
  // If not in ACTIVE phase, just update last piezo state and exit
  if (phase != ACTIVE_PHASE) {
    lastPiezoState = digitalRead(SWITCH2_PIN); // Read current piezo state
    return;
  }

  int p = digitalRead(SWITCH2_PIN); // Read current piezo input

  // Detect falling edge (HIGH to LOW) and debounce
  if (lastPiezoState == HIGH && p == LOW && (now - lastPiezo) > PIEZO_DEBOUNCE) {
    motor2Hits++; // Increment hit counter
    lastPiezo = now; // Update last hit timestamp
    Serial.print("Piezo hit count: "); // Debug output
    Serial.println(motor2Hits); // Show current hit count

    // Stop Motor2 if hit count reaches target
    if (motor2Hits >= MOTOR2_STOP_TARGET)
      motor2Running = false;
  }

  lastPiezoState = p; // Update last piezo state for next check
}

// ==== Electromagnet Activation ====
// Turns ON the electromagnet and records the start time.
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW); // Activate electromagnet (LOW = ON)
  magnetActive = true;                  // Set magnet state to active
  magnetStart = millis();               // Record the time magnet was turned on
}

// ==== Electromagnet Deactivation after 10s ====
// Turns OFF the electromagnet after 10 seconds.
void updateMagnet(unsigned long now) {
  // Check if magnet is active and 10 seconds have passed
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH); // Deactivate magnet (HIGH = OFF)
    magnetActive = false;                  // Reset magnet state
  }
}

// ==== Scroll "READY" Message ====
// Scrolls the word "READY" across the 4-digit display.
void scrollReadyLoop() {
  unsigned long now = millis(); // Get current time
  // Update scroll every 200ms
  if (now - lastReadyScroll >= 200) {
    lastReadyScroll = now; // Save scroll timestamp
    const int len = 5;     // Length of "READY" message
    uint8_t frame[4];      // Frame buffer for 4 digits

    // Fill frame with rotated characters or blanks
    for (int i = 0; i < 4; i++) {
      int idx = readyScrollIndex - i;
      frame[i] = (idx >= 0 && idx < len) ? rotate180(MSG_READY[idx]) : MY_SEG_BLANK;
    }

    display.setSegments(frame); // Show frame on display
    readyScrollIndex++;         // Move scroll position

    // Reset scroll index after message passes through
    if (readyScrollIndex > (len + 3)) readyScrollIndex = -4;
  }
}

// ==== Scroll Custom Message ====
// Scrolls any custom message across the display, one frame at a time.
void scrollMessage180(const uint8_t* msg, int len, int delayMs) {
  // Scroll from right to left across the display
  for (int i = len + 3; i >= 0; i--) {
    uint8_t frame[4] = { 0 }; // Clear frame buffer

    // Fill frame with rotated characters or blanks
    for (int j = 0; j < 4; j++) {
      int idx = i - j;
      frame[3 - j] = (idx >= 0 && idx < len) ? rotate180(msg[len - idx - 1]) : MY_SEG_BLANK;
    }

    display.setSegments(frame); // Show frame on display
    delay(delayMs);             // Wait before next scroll step
  }
}

// ==== Reset System to Initial READY State ====
// Resets all counters, stops motors, and returns to READY phase.
void resetSystem() {
  countdown = 0;                     // Reset countdown timer
  cycle = 0;                         // Reset cycle counter
  phase = READY_PHASE;              // Set phase to READY
  stopMotors();                     // Stop all motors
  magnetActive = false;             // Reset magnet state
  digitalWrite(ELECTROMAGNET_PIN, HIGH); // Ensure magnet is OFF
  lastTick = millis();              // Reset tick timer
  readyScrollIndex = -4;            // Reset scroll position
  showTimeCycleWithHits(countdown, cycle, motor2Hits, millis()); // Update display
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
  const unsigned long DEBOUNCE_DELAY = 50; // Debounce threshold in milliseconds
  static int lastRawStart = HIGH;          // Previous raw state of start button
  static int stableStart = HIGH;           // Debounced stable state
  static unsigned long lastStartDebounce = 0; // Last time the button state changed
  static bool startHandled = false;        // Flag to prevent repeated handling of same press

  int rawStart = digitalRead(SWITCH_START); // Read current state of start button
  if (rawStart != lastRawStart) {
    lastStartDebounce = now; // Reset debounce timer
    startHandled = false;    // Allow new press to be handled
  }
  lastRawStart = rawStart; // Update previous raw state

  // Check if button state is stable after debounce delay
  if ((now - lastStartDebounce) > DEBOUNCE_DELAY) {
    if (rawStart != stableStart) {
      stableStart = rawStart; // Update stable state
      if (stableStart == LOW && !startHandled) {
        startHandled = true; // Mark this press as handled

        // --- Detect single or double press ---
        // If pressed twice within interval, treat as double press
        if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
        else startCount = 1;
        lastStartTime = now;

        if (startCount >= 2) {
          // Double press: reset system
          stopMotors(); // Stop all motors
          const uint8_t MSG_RESET[] = { MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T };
          scrollMessage180(MSG_RESET, 5, 200); // Scroll "RESET" message
          resetSystem(); // Reinitialize system
          startCount = 0; // Clear press count
        } else {
          // Single press: toggle between READY and COOL phase
          if (phase == READY_PHASE) {
            phase = COOL_PHASE; // Move to COOL phase
            countdown = CYCLE_INTERVAL / 1000; // Set countdown for COOL phase
            lastTick = now; // Record tick time
            stopMotors(); // Ensure motors are off
          } else {
            phase = READY_PHASE; // Return to READY phase
            countdown = 0;
            stopMotors();
            readyScrollIndex = -4; // Reset scroll position
          }
        }
      }
    }
  }

  // --- Phase Control ---
  switch (phase) {
    case READY_PHASE:
      // Show scrolling "READY" message
      stopMotors();       // Ensure motors are off
      scrollReadyLoop();  // Scroll "READY" on display
      break;

    case COOL_PHASE:
      // Rest phase between active cycles
      stopMotors();       // Ensure motors are off
      motor1AutoTriggered = false; // Reset auto-trigger flag for Motor1

      // Countdown timer for COOL phase
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          // Transition to ACTIVE phase
          phase = ACTIVE_PHASE;
          countdown = CYCLE_ACTIVE / 1000; // Set countdown for ACTIVE phase
          cycle++; // Increment cycle count
          motor2Running = true; // Start Motor2
          motor2Hits = 0;       // Reset piezo hit counter
          activateMagnet();     // Turn on electromagnet
          activePhaseStart = now; // Record start time of ACTIVE phase
        }
      }
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now); // Update display
      break;

    case ACTIVE_PHASE:
      // Main working phase: motors run, piezo counts hits

      // Countdown timer for ACTIVE phase
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          motor1AutoTriggered = false; // Reset auto-trigger flag
          if (cycle >= MAX_CYCLES) {
            // All cycles complete: show "FINISH" and reset
            stopMotors();
            const uint8_t MSG_FINISH[] = { MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H };
            scrollMessage180(MSG_FINISH, 6, 300); // Scroll "FINISH"
            resetSystem(); // Reinitialize system
            break;
          }
          // Move to COOL phase
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000;
          stopMotors();
        }
      }

      // --- Debounce for Motor1 Button ---
      const unsigned long DEBOUNCE_S1 = 50; // Debounce threshold for Motor1 button
      static int lastRawS1 = HIGH;          // Previous raw state
      static unsigned long lastS1Debounce = 0; // Last debounce timestamp

      int rawS1 = digitalRead(SWITCH1_PIN); // Read Motor1 button
      if (rawS1 != lastRawS1) {
        lastS1Debounce = now; // Reset debounce timer
      }
      lastRawS1 = rawS1;

      // Manual trigger for Motor1
      if (digitalRead(SWITCH1_PIN) == LOW) {
        motor1Start = now;     // Record start time
        motor1Active = true;   // Activate Motor1
      }

      // Auto-trigger Motor1 at 20s remaining (once per cycle)
      if (!motor1Active && !motor1AutoTriggered && countdown == 20) {
        motor1Start = now;     // Record start time
        motor1Active = true;   // Activate Motor1
        motor1AutoTriggered = true; // Prevent retriggering in same cycle
      }

      // Update Motor1 status (stop if time expired or phase changed)
      updateMotor1(now);

      // Stop Motor2 if hit target or time is almost up
      if (motor2Running && (motor2Hits >= MOTOR2_STOP_TARGET || (now - activePhaseStart >= CYCLE_ACTIVE - 10000))) {
        motor2Running = false; // Stop Motor2
      }

      // Update Motor2 and other components
      driveMotor2();          // Control Motor2 output
      checkMotor2Piezo(now);  // Check piezo sensor for hits
      updateMagnet(now);      // Turn off magnet if time expired
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now); // Update display
      break;
  }
}
