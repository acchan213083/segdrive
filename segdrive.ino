/*
MIT License

Copyright (c) 2025 Namabayashi

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

/*
Arduino Timing Controller Full Explanation:

- Single electromagnet: OFF for 5 seconds at cycle start, then ON
- Motor1: relay-driven, triggered by momentary switch for 10 seconds
- Motor2: PWM-controlled, stopped via piezo sensor after 5 hits
- LED: indicates Motor1 active or piezo detection
- TM1637 4-digit display shows countdown (left 2 digits) and cycle count (right 2 digits)
- System runs 10 cycles, then scrolls "FIN" on display and stops
- Start button (4-terminal) initiates system
*/

#include <TM1637Display.h>

// ============================
// Pin Configuration
// ============================
#define ELECTROMAGNET_PIN 2       // Electromagnet output
#define CLK 4                     // TM1637 Clock
#define DIO 5                     // TM1637 Data
#define SWITCH_START 6            // Start button (4-terminal)
#define SWITCH1_PIN 7             // Motor1 trigger button
#define SWITCH2_PIN 8             // Piezo sensor for Motor2 stop
#define MOTOR1_RELAY_PIN 9        // Motor1 relay control
#define MOTOR2_RELAY_PIN 10       // Motor2 relay control
#define MOTOR2_IN1 11             // Motor2 PWM IN1
#define MOTOR2_IN2 12             // Motor2 PWM IN2
#define LED 13                    // Status LED (Motor1 / Piezo detection)

// Create display object
TM1637Display display(CLK, DIO);

// ============================
// Timing and Configuration Variables
// ============================
const unsigned long MAGNET_OFF_DURATION     = 10000;    // Electromagnet OFF duration at cycle start (ms)
const unsigned long CYCLE_TOTAL_DURATION    = 30000;   // Active phase duration per cycle (ms)
const unsigned long MOTOR_ON_DURATION       = 10000;   // Motor1 run time (ms)
const unsigned long MAGNET_INITIAL_INTERVAL = 5000;    // Pre-start interval before cycles begin (ms)
const int  MOTOR2_STOP_TARGET               = 5;       // Piezo hits required to stop Motor2
const int  MAX_CYCLES                       = 10;      // Total number of cycles
const unsigned long PIEZO_COOLDOWN          = 200;     // Piezo debounce duration (ms)
const int  MOTOR2_PWM_POWER                  = 255;     // Motor2 PWM level (0-255)
const unsigned long ROTATE_INTERVAL         = 200;     // '0' rotation interval on display (ms)

// ============================
// State Variables
// ============================
bool systemRunning       = false;             // True after start button pressed
bool state               = false;             // false=interval, true=active phase
int t                    = 5;                 // Countdown seconds
int cycle                = 0;                 // Current cycle number

bool magnetActive        = false;             // Flag for OFF interval
unsigned long magnetStart = 0;                // Timestamp when electromagnet turned OFF

bool motor1Active        = false;             // Motor1 active flag
unsigned long motor1StartTime = 0;           // Motor1 start timestamp

bool motor2Running       = false;             // Motor2 running flag
int motor2StopCount      = 0;                 // Motor2 Piezo hit counter

unsigned long lastTick   = 0;                 // Timer for countdown

int lastPiezoState       = HIGH;              // Last piezo sensor state
unsigned long lastPiezoDetectTime = 0;        // Timestamp for last Piezo hit

// ============================
// Setup
// ============================
void setup() {
  Serial.begin(9600);

  // ----------------------------
  // Set pin modes
  // ----------------------------
  pinMode(ELECTROMAGNET_PIN, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);
  pinMode(MOTOR2_IN1, OUTPUT);
  pinMode(MOTOR2_IN2, OUTPUT);

  pinMode(SWITCH_START, INPUT_PULLUP); // Start button with internal pull-up
  pinMode(SWITCH1_PIN, INPUT_PULLUP);  // Motor1 button
  pinMode(SWITCH2_PIN, INPUT);         // Piezo sensor (no pull-up)

  // ----------------------------
  // Initialize outputs
  // ----------------------------
  digitalWrite(ELECTROMAGNET_PIN, HIGH);  // Electromagnet initially ON
  digitalWrite(LED, LOW);
  digitalWrite(MOTOR1_RELAY_PIN, LOW);
  digitalWrite(MOTOR2_RELAY_PIN, LOW);

  // ----------------------------
  // Display setup
  // ----------------------------
  display.setBrightness(7);  // Max brightness
  showTimeAndCycle(t, cycle);

  lastTick = millis();

  Serial.println("Setup complete. Waiting for start button...");
}

// ============================
// Main Loop
// ============================
void loop() {
  unsigned long now = millis();

  // ----------------------------
  // Wait for start button press
  // ----------------------------
  if (!systemRunning) {
    if (digitalRead(SWITCH_START) == LOW) {
      systemRunning = true;
      Serial.println("Start button pressed. System starting...");
      delay(300); // debounce
      t = 5;
      cycle = 0;
      state = false;
      display.showNumberDec(0, true);
      delay(MAGNET_INITIAL_INTERVAL); // Pre-start interval
    } else {
      return; // Wait until start button pressed
    }
  }

  // ----------------------------
  // Countdown logic: non-blocking
  // ----------------------------
  if (now - lastTick >= 1000) {
    lastTick = now;
    t -= 1;  // decrement seconds
    showTimeAndCycle(t, cycle);

    if (t <= 0) {
      if (state) {
        // Active phase ended
        if (cycle >= MAX_CYCLES) {
          finishScroll();  // Scroll "FIN"
          while(true);     // Stop program
        } else {
          state = false;
          t = 5; // interval phase
        }
      } else {
        // Interval phase ended -> start new active cycle
        activateMagnet();   // Electromagnet OFF for 5s
        cycle += 1;
        state = true;
        t = CYCLE_TOTAL_DURATION / 1000; // Active phase seconds
        motor2Running = true;
        motor2StopCount = 0;             // Reset piezo counter
        Serial.print("Cycle ");
        Serial.print(cycle);
        Serial.println(" started.");
      }
    }
  }

  // ----------------------------
  // Update actuators and sensors
  // ----------------------------
  updateMagnet(now);      // Electromagnet timing
  driveSecondMotor();     // Motor2 PWM
  updateMotor1(now);      // Motor1 control
  checkMotor2Stop();      // Piezo detection for Motor2 stop
}

// ============================
// Electromagnet Control
// ============================
void activateMagnet() {
  Serial.println("Electromagnet OFF (5 sec at cycle start)");
  digitalWrite(ELECTROMAGNET_PIN, LOW); // Turn OFF temporarily
  magnetActive = true;
  magnetStart = millis();               // Record timestamp
}

void updateMagnet(unsigned long now) {
  // Restore electromagnet after OFF duration
  if (magnetActive && now - magnetStart >= MAGNET_OFF_DURATION) {
    Serial.println("Electromagnet ON (restored)");
    digitalWrite(ELECTROMAGNET_PIN, HIGH);
    magnetActive = false;
  }
}

// ============================
// Motor1 Control
// ============================
void updateMotor1(unsigned long now) {
  // Trigger Motor1 if button pressed and active phase
  if (state && digitalRead(SWITCH1_PIN) == LOW && !motor1Active) {
    motor1Active = true;
    motor1StartTime = now;
    digitalWrite(MOTOR1_RELAY_PIN, HIGH); // Relay ON
    digitalWrite(LED, HIGH);              // LED ON
    Serial.println("Motor1 ON (10 sec)");
  }

  // Turn off Motor1 after duration
  if (motor1Active && now - motor1StartTime >= MOTOR_ON_DURATION) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
    digitalWrite(LED, LOW);
    Serial.println("Motor1 OFF");
  }
}

// ============================
// Motor2 PWM Control
// ============================
void driveSecondMotor() {
  if (motor2Running) {
    analogWrite(MOTOR2_IN1, MOTOR2_PWM_POWER);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, HIGH); // Relay ON while running
  } else {
    analogWrite(MOTOR2_IN1, 0);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, LOW);  // Relay OFF when stopped
  }
}

// ============================
// Piezo Sensor Detection for Motor2 Stop
// ============================
void checkMotor2Stop() {
  int piezoState = digitalRead(SWITCH2_PIN);

  // Detect HIGH -> LOW transition
  if (lastPiezoState == HIGH && piezoState == LOW) {
    unsigned long now = millis();
    if (now - lastPiezoDetectTime > PIEZO_COOLDOWN) {
      motor2StopCount++;                  // Increment hit counter
      Serial.print("Motor2 Piezo hit: ");
      Serial.println(motor2StopCount);
      digitalWrite(LED, HIGH);            // LED ON briefly
      lastPiezoDetectTime = now;

      // Stop Motor2 after 5 hits
      if (motor2StopCount >= MOTOR2_STOP_TARGET) {
        motor2Running = false;            // Stop PWM
        digitalWrite(MOTOR2_RELAY_PIN, LOW); // Relay OFF
        Serial.println("Motor2 stopped via Piezo relay after 5 hits.");
      }
    }
  }

  lastPiezoState = piezoState;

  // Turn off LED after cooldown
  if (millis() - lastPiezoDetectTime > PIEZO_COOLDOWN) {
    digitalWrite(LED, LOW);
  }
}

// ============================
// Display Control
// ============================
void showTimeAndCycle(int seconds, int cycle) {
  uint8_t digits[] = {
    display.encodeDigit((seconds / 10) % 10), // Tens place of seconds
    display.encodeDigit(seconds % 10),        // Ones place of seconds
    display.encodeDigit((cycle / 10) % 10),   // Tens place of cycle
    display.encodeDigit(cycle % 10)           // Ones place of cycle
  };
  display.setSegments(digits);
}

// ============================
// Scroll "FIN" at end of 10 cycles
// ============================
void finishScroll() {
  const uint8_t CHAR_F = 0b01110001;
  const uint8_t CHAR_I = 0b00010000;
  const uint8_t CHAR_N = 0b01010100;
  const uint8_t CHAR_BLANK = 0x00;

  const uint8_t message[] = { CHAR_F, CHAR_I, CHAR_N };
  const int len = sizeof(message);

  for (int i = 0; i <= len + 3; i++) {
    uint8_t frame[4] = {
      (i >= 3 && i - 3 < len) ? message[i - 3] : CHAR_BLANK,
      (i >= 2 && i - 2 < len) ? message[i - 2] : CHAR_BLANK,
      (i >= 1 && i - 1 < len) ? message[i - 1] : CHAR_BLANK,
      (i < len)               ? message[i]     : CHAR_BLANK
    };
    display.setSegments(frame);
    delay(300);
  }

  uint8_t final[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_BLANK };
  display.setSegments(final);

  // Turn off all actuators
  digitalWrite(ELECTROMAGNET_PIN, HIGH);
  digitalWrite(MOTOR1_RELAY_PIN, LOW);
  digitalWrite(MOTOR2_RELAY_PIN, LOW);
  digitalWrite(LED, LOW);

  delay(30000);  // Wait before clearing display
  display.clear();
}
