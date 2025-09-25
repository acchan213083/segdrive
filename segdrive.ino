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
#define SWITCH_PIN 7  // 追加：モーターを動かすスイッチ

TM1637Display display(CLK, DIO);

const unsigned long MAGNET_ON_DURATION = 5000; // 電磁石のON時間
const unsigned long MOTOR_ON_DURATION = 10000; // スイッチ押下後モーターON時間

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

int motorPower1 = 255; // PWM for second motor
int motorPower2 = 255; // PWM for second motor

const uint8_t CHAR_F     = 0b01110001;
const uint8_t CHAR_I     = 0b00010000;
const uint8_t CHAR_N     = 0b01010100;
const uint8_t CHAR_S     = 0b01101101;
const uint8_t CHAR_H     = 0b01110100;
const uint8_t CHAR_BLANK = 0x00;

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

  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  // ----- スイッチ押下でIN1/IN2モーター制御 -----
  if (digitalRead(SWITCH_PIN) == LOW && !motorActive) {
    motorActive = true;
    motorStartTime = now;
    analogWrite(IN1, 255);
    analogWrite(IN2, 0);
    Serial.println("Motor (IN1/IN2) ON for 10s");
  }

  if (motorActive && now - motorStartTime >= MOTOR_ON_DURATION) {
    motorActive = false;
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
    Serial.println("Motor (IN1/IN2) OFF");
  }

  // ----- 元のプログラムのループ処理 -----
  if (now - lastTick >= (unsigned long)(1000 / speedFactor)) {
    lastTick = now;
    t -= 1;
    showTimeAndCycle(t, cycle);

    if (t == 0) {
      if (state == true) {
        if (cycle >= 10) {
          finishScroll();
          while (true);
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

  driveSecondMotor();
  updateMagnets(now);
}

// 元のプログラムの2台目モーター制御（IN3/IN4）
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
  if (magnet1Active && now > magnet1Start && now - magnet1Start >= MAGNET_ON_DURATION) {
    Serial.println("Magnet1 OFF");
    digitalWrite(electromagnet1, LOW);
    magnet1Active = false;
  }
  if (magnet2Active && now > magnet2Start && now - magnet2Start >= MAGNET_ON_DURATION) {
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

  // モータと電磁石を停止
  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  delay(30000); // 30秒待機してディスプレイを消灯
  display.clear();
}
