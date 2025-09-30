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

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10
#define CLK 2
#define DIO 3
#define LED 13
#define electromagnet1 8
#define electromagnet2 11
#define SWITCH_PIN 7
#define MOTOR1_STATUS_PIN 4
#define MOTOR2_STATUS_PIN 12

TM1637Display display(CLK, DIO);

const unsigned long MAGNET_ON_DURATION = 5000;
const unsigned long MOTOR_ON_DURATION = 10000;

bool state = false;
int t = 5;
int cycle = 0;
float speedFactor = 1.0;

unsigned long lastTick = 0;

bool magnet1Active = false;
bool magnet2Active = false;
unsigned long magnet1Start = 0;
unsigned long magnet2Start = 0;

bool motorActive = false;
unsigned long motorStartTime = 0;

int motorPower1 = 255;
int motorPower2 = 255;

const uint8_t CHAR_F     = 0b01110001;
const uint8_t CHAR_I     = 0b00010000;
const uint8_t CHAR_N     = 0b01010100;
const uint8_t CHAR_S     = 0b01101101;
const uint8_t CHAR_H     = 0b01110100;
const uint8_t CHAR_BLANK = 0x00;

const uint8_t SEG_0 = 0b00111111;
const uint8_t SEG_ROTATE[6] = {
  0b00111110, 0b00111101, 0b00111011,
  0b00110111, 0b00101111, 0b00011111
};

unsigned long lastRotate = 0;
int rotateIndex = 0;
const unsigned long ROTATE_INTERVAL = 200;

void setup() {
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(electromagnet1, OUTPUT);
  pinMode(electromagnet2, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MOTOR1_STATUS_PIN, OUTPUT);
  pinMode(MOTOR2_STATUS_PIN, OUTPUT);

  digitalWrite(IN1, 0);
  digitalWrite(IN2, 0);
  digitalWrite(IN3, 0);
  digitalWrite(IN4, 0);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);
  digitalWrite(MOTOR1_STATUS_PIN, LOW);
  digitalWrite(MOTOR2_STATUS_PIN, LOW);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  if (state && digitalRead(SWITCH_PIN) == LOW) {
    motorActive = true;
    motorStartTime = now;
    analogWrite(IN1, 255);
    analogWrite(IN2, 0);
    digitalWrite(LED, HIGH);
    digitalWrite(MOTOR1_STATUS_PIN, HIGH);
    Serial.println("Motor1/LED ON for 10s");
  }

  if (motorActive && (!state || now - motorStartTime >= MOTOR_ON_DURATION)) {
    motorActive = false;
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
    digitalWrite(LED, LOW);
    digitalWrite(MOTOR1_STATUS_PIN, LOW);
    Serial.println("Motor1/LED OFF");
  }

  if (motorActive && now - lastRotate >= ROTATE_INTERVAL) {
    lastRotate = now;
    rotateIndex = (rotateIndex + 1) % 6;

    uint8_t digits[4];
    digits[0] = display.encodeDigit((t / 10) % 10);
    digits[1] = display.encodeDigit(t % 10);
    digits[2] = display.encodeDigit((cycle / 10) % 10);
    digits[3] = display.encodeDigit(cycle % 10);

    int targetDigit = (cycle == 10) ? 3 : (cycle >= 1 ? 2 : 0);
    if (digits[targetDigit] == SEG_0) {
      digits[targetDigit] = SEG_ROTATE[rotateIndex];
    }

    display.setSegments(digits);
  }

  if (now - lastTick >= (unsigned long)(1000 / speedFactor)) {
    lastTick = now;
    t -= 1;
    showTimeAndCycle(t, cycle);

    if (t == 0) {
      if (state) {
        if (cycle >= 10) {
          finishScroll();
          while (true);
        } else {
          activateMagnet2();
          state = false;
          t = 5;
          if (motorActive) {
            motorActive = false;
            analogWrite(IN1, 0);
            analogWrite(IN2, 0);
            digitalWrite(LED, LOW);
            digitalWrite(MOTOR1_STATUS_PIN, LOW);
            Serial.println("Motor1/LED OFF due to interval");
          }
        }
      } else {
        activateMagnet1();
        cycle += 1;
        state = true;
        t = 40;
      }
    }
  }

  driveSecondMotor();
  updateMagnets(now);
}

void driveSecondMotor() {
  if (state) {
    analogWrite(IN3, motorPower1);
    analogWrite(IN4, 0);
    digitalWrite(MOTOR2_STATUS_PIN, HIGH);
  } else {
    analogWrite(IN3, 0);
    analogWrite(IN4, 0);
    digitalWrite(MOTOR2_STATUS_PIN, LOW);
  }
}

void showTimeAndCycle(int seconds, int cycle) {
  uint8_t digits[] = {
    display.encodeDigit((seconds / 10) % 10),
    display.encodeDigit(seconds % 10),
    display.encodeDigit((cycle / 10) % 10),
    display.encodeDigit(cycle % 10)
  };
  display.setSegments(digits);
}

void activateMagnet1() {
  Serial.println("Magnet1 ON");
  digitalWrite(electromagnet1, HIGH);
  magnet1Active = true;
  magnet1Start = millis();
  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true);
}

void activateMagnet2() {
  Serial.println("Magnet2 ON");
  digitalWrite(electromagnet2, HIGH);
  magnet2Active = true;
  magnet2Start = millis();
  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true);
}

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
    delay(300);
  }

  uint8_t final[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_BLANK };
  display.setSegments(final);

  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);
  digitalWrite(MOTOR1_STATUS_PIN, LOW);
  digitalWrite(MOTOR2_STATUS_PIN, LOW);

  delay(30000);
  display.clear();
}
