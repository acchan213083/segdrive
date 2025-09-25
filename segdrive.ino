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

#include <TM1637Display.h>

// Motor and device pin definitions
#define IN1 5           // Motor 1 input 1 (controlled by switch)
#define IN2 6           // Motor 1 input 2 (controlled by switch)
#define IN3 9           // Motor 2 input 1 (PWM motor)
#define IN4 10          // Motor 2 input 2
#define CLK 2           // TM1637 clock pin
#define DIO 3           // TM1637 data pin
#define LED 13          // Indicator LED for motor 2
#define electromagnet1 8
#define electromagnet2 11
#define SWITCH_PIN 7    // Input pin for momentary switch

TM1637Display display(CLK, DIO);

// Timing constants
const unsigned long MAGNET_ON_DURATION = 5000;  // Electromagnet active duration (ms)
const unsigned long MOTOR_ON_DURATION = 10000;  // Motor 1 active duration after switch press (ms)

// Program state variables
bool state = false;      // Motor 2 state flag
int t = 5;               // Countdown seconds for display
int cycle = 0;           // Cycle counter
float speedFactor = 1.0; // Speed factor for countdown timing

unsigned long lastTick = 0;  // Last update timestamp

// Electromagnet control variables
bool magnet1Active = false;
bool magnet2Active = false;
unsigned long magnet1Start = 0;
unsigned long magnet2Start = 0;

// Motor 1 control variables (switch-controlled)
bool motorActive = false;
unsigned long motorStartTime = 0;

// Motor 2 PWM power levels
int motorPower1 = 255;
int motorPower2 = 255;

// 7-segment custom character definitions
const uint8_t CHAR_F     = 0b01110001;
const uint8_t CHAR_I     = 0b00010000;
const uint8_t CHAR_N     = 0b01010100;
const uint8_t CHAR_S     = 0b01101101;
const uint8_t CHAR_H     = 0b01110100;
const uint8_t CHAR_BLANK = 0x00;

void setup() {
  Serial.begin(9600);

  // Initialize motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Initialize other outputs
  pinMode(LED, OUTPUT);
  pinMode(electromagnet1, OUTPUT);
  pinMode(electromagnet2, OUTPUT);

  // Initialize switch input with pull-up resistor
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Ensure all devices are off initially
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  // Initialize display
  display.setBrightness(7);
  showTimeAndCycle(t, cycle);

  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  // ----- Switch-controlled motor 1 -----
  // If the switch is pressed and motor is inactive, start motor
  if (digitalRead(SWITCH_PIN) == LOW && !motorActive) {
    motorActive = true;
    motorStartTime = now;
    analogWrite(IN1, 255);
    analogWrite(IN2, 0);
    Serial.println("Motor (IN1/IN2) ON for 10s");
  }

  // Stop motor 1 after 10 seconds
  if (motorActive && now - motorStartTime >= MOTOR_ON_DURATION) {
    motorActive = false;
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
    Serial.println("Motor (IN1/IN2) OFF");
  }

  // ----- Original loop behavior for motor 2 and cycle timing -----
  if (now - lastTick >= (unsigned long)(1000 / speedFactor)) {
    lastTick = now;
    t -= 1;
    showTimeAndCycle(t, cycle);

    if (t == 0) {
      if (state == true) {
        if (cycle >= 10) {
          finishScroll();
          while (true); // Halt program after finishing cycles
        } else {
          activateMagnet2();
          state = false;
          t = 5;
        }
      } else {
        activateMagnet1();
        cycle += 1;
        state = true;
        t = 40;
      }
    }
  }

  // Drive motor 2 and update electromagnets
  driveSecondMotor();
  updateMagnets(now);
}

// Control motor 2 (IN3/IN4) based on state
void driveSecondMotor() {
  if (state) {
    analogWrite(IN3, motorPower1);
    analogWrite(IN4, 0);
    digitalWrite(LED, HIGH);
  } else {
    analogWrite(IN3, 0);
    analogWrite(IN4, 0);
    digitalWrite(LED, LOW);
  }
}

// Update 7-segment display with countdown seconds and cycle number
void showTimeAndCycle(int seconds, int cycle) {
  uint8_t digits[] = {
    display.encodeDigit((seconds / 10) % 10),
    display.encodeDigit(seconds % 10),
    display.encodeDigit((cycle / 10) % 10),
    display.encodeDigit(cycle % 10)
  };
  display.setSegments(digits);
}

// Activate electromagnet 1
void activateMagnet1() {
  Serial.println("Magnet1 ON");
  digitalWrite(electromagnet1, HIGH);
  magnet1Active = true;
  magnet1Start = millis();

  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true); // Show colon while magnet is active
}

// Activate electromagnet 2
void activateMagnet2() {
  Serial.println("Magnet2 ON");
  digitalWrite(electromagnet2, HIGH);
  magnet2Active = true;
  magnet2Start = millis();

  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true); // Show colon while magnet is active
}

// Update electromagnet status based on elapsed time
void updateMagnets(unsigned long now) {
  if (magnet1Active && now - magnet1Start >= MAGNET_ON_DURATION) {
    Serial.println("Magnet1 OFF");
    digitalWrite(electromagnet1, LOW);
    magnet1Active = false;
  }
  if (magnet2Active && now - magnet2Start >= MAGNET_ON_DURATION) {
    Serial.println("Magnet2 OFF");
    digitalWrite(electromagnet2, LOW);
    magnet2Active = false;
  }
}

// Scroll "FINISH" on the 7-segment display and turn off all devices
void finishScroll() {
  const uint8_t message[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_I, CHAR_S, CHAR_H };
  const int len = sizeof(message);

  for (int i = 0; i <= len + 3; i++) {
    uint8_t frame[4] = {
      (i >= 3 && i - 3 < len) ? message[i - 3] : CHAR_BLANK,
      (i >= 2 && i - 2 < len) ? message[i - 2] : CHAR_BLANK,
      (i >= 1 && i - 1 < len) ? message[i - 1] : CHAR_BLANK,
      (i < len)               ? message[i]     : CHAR_BLANK
    };
    display.setSegments(frame);
    delay(300); // Scroll speed
  }

  uint8_t final[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_BLANK };
  display.setSegments(final);

  // Stop all motors and electromagnets
  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  delay(30000); // Wait 30 seconds before clearing display
  display.clear();
}
