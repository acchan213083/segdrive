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

// ==== ピン設定 ====
#define CLK 4
#define DIO 5
#define SWITCH_START 6
#define SWITCH1_PIN 7
#define SWITCH2_PIN 8
#define MOTOR1_RELAY_PIN 9
#define MOTOR2_RELAY_PIN 10
#define MOTOR2_IN1 11
#define MOTOR2_IN2 12
#define ELECTROMAGNET_PIN 2
#define LED 13

TM1637Display display(CLK, DIO);

// ==== 7セグ文字定義 ====
const uint8_t MY_SEG_A = 0b01110111;
const uint8_t MY_SEG_b = 0b01111100;
const uint8_t MY_SEG_C = 0b00111001;
const uint8_t MY_SEG_d = 0b01011110;
const uint8_t MY_SEG_D = 0b01011110;
const uint8_t MY_SEG_E = 0b01111001;
const uint8_t MY_SEG_F = 0b01110001;
const uint8_t MY_SEG_H = 0b01110110;
const uint8_t MY_SEG_I = 0b00000110;
const uint8_t MY_SEG_L = 0b00111000;
const uint8_t MY_SEG_N = 0b01010100;
const uint8_t MY_SEG_P = 0b01110011;
const uint8_t MY_SEG_R = 0b01010000;
const uint8_t MY_SEG_S = 0b01101101;
const uint8_t MY_SEG_T = 0b01111000;
const uint8_t MY_SEG_U = 0b00111110;
const uint8_t MY_SEG_Y = 0b01101110;
const uint8_t MY_SEG_0 = 0b00111111;
const uint8_t MY_SEG_BLANK = 0x00;

// ==== タイミング定数 ====
const unsigned long MOTOR1_DURATION = 10000;
const unsigned long MOTOR2_PWM = 255;
const unsigned long PIEZO_DEBOUNCE = 100;
const int MOTOR2_STOP_TARGET = 5;
const int MAX_CYCLES = 10;
const unsigned long CYCLE_ACTIVE = 30000;
const unsigned long CYCLE_INTERVAL = 5000;

// ==== 状態変数 ====
bool motor1Active = false;
unsigned long motor1Start = 0;
bool motor2Running = false;
int motor2Hits = 0;
unsigned long lastPiezo = 0;
int lastPiezoState = HIGH;
unsigned long lastTick = 0;
int countdown = 0;
int cycle = 0;
enum Phase {READY_PHASE, COOL_PHASE, ACTIVE_PHASE} phase = READY_PHASE;
bool magnetActive = false;
unsigned long magnetStart = 0;

// READYスクロール用
unsigned long lastReadyScroll = 0;
int readyScrollIndex = -4;
const uint8_t MSG_READY[] = {MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y};

// STARTボタン2回押し判定用
unsigned long lastStartTime = 0;
int startCount = 0;
const unsigned long DOUBLE_PRESS_INTERVAL = 500;

// ==== 180°回転マッピング ====
uint8_t rotate180(uint8_t seg) {
  uint8_t rotated = 0;
  if (seg & 0b00000001) rotated |= 0b00001000;
  if (seg & 0b00000010) rotated |= 0b00010000;
  if (seg & 0b00000100) rotated |= 0b00100000;
  if (seg & 0b00001000) rotated |= 0b00000001;
  if (seg & 0b00010000) rotated |= 0b00000010;
  if (seg & 0b00100000) rotated |= 0b00000100;
  if (seg & 0b01000000) rotated |= 0b01000000;
  return rotated;
}

// ==== 表示関数 ====
void showTimeCycle180(int seconds, int cyc) {
  int s1 = seconds / 10;
  int s0 = seconds % 10;
  int c1 = cyc / 10;
  int c0 = cyc % 10;
  uint8_t digits[4] = {
    rotate180(display.encodeDigit(c0)),
    rotate180(display.encodeDigit(c1)),
    rotate180(display.encodeDigit(s0)),
    rotate180(display.encodeDigit(s1))
  };
  display.setSegments(digits);
}

// ==== モータ停止関数 ====
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

// ==== READYスクロール ====
void scrollReadyLoop() {
  unsigned long now = millis();
  if (now - lastReadyScroll >= 200) {
    lastReadyScroll = now;
    uint8_t frame[4];
    for (int i = 0; i < 4; i++) {
      int idx = readyScrollIndex + i;
      if (idx >= 0 && idx < 5) frame[i] = rotate180(MSG_READY[idx]);
      else frame[i] = MY_SEG_BLANK;
    }
    display.setSegments(frame);
    readyScrollIndex--;
    if (readyScrollIndex < -4) readyScrollIndex = 5;
  }
}

// ==== メッセージスクロール ====
void scrollMessage180(const uint8_t* msg, int len, int delayMs) {
  for (int i = len + 3; i >= 0; i--) {
    uint8_t frame[4] = {0};
    for (int j = 0; j < 4; j++) {
      int idx = i - j;
      if (idx >= 0 && idx < len) frame[3 - j] = rotate180(msg[len - idx - 1]);
      else frame[3 - j] = MY_SEG_BLANK;
    }
    display.setSegments(frame);
    delay(delayMs);
  }
}

// ==== システムリセット ====
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

// ==== 電磁石制御 ====
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW);
  magnetActive = true;
  magnetStart = millis();
}
void updateMagnet(unsigned long now) {
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH);
    magnetActive = false;
  }
}

// ==== Motor制御 ====
void updateMotor1(unsigned long now) {
  if (motor1Active && now - motor1Start >= MOTOR1_DURATION) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
  }
}
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
void checkMotor2Piezo(unsigned long now) {
  if (phase != ACTIVE_PHASE) return;
  int p = digitalRead(SWITCH2_PIN);
  if (lastPiezoState == HIGH && p == LOW && now - lastPiezo > PIEZO_DEBOUNCE) {
    motor2Hits++;
    lastPiezo = now;
    if (motor2Hits >= MOTOR2_STOP_TARGET) motor2Running = false;
  }
  lastPiezoState = p;
}

// ==== セットアップ ====
void setup() {
  pinMode(SWITCH_START, INPUT_PULLUP);
  pinMode(SWITCH1_PIN, INPUT_PULLUP);
  pinMode(SWITCH2_PIN, INPUT);
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);
  pinMode(MOTOR2_IN1, OUTPUT);
  pinMode(MOTOR2_IN2, OUTPUT);
  pinMode(ELECTROMAGNET_PIN, OUTPUT);
  pinMode(LED, OUTPUT);

  stopMotors();
  digitalWrite(ELECTROMAGNET_PIN, HIGH);

  display.setBrightness(7);
  resetSystem();
}

// ==== メインループ ====
void loop() {
  unsigned long now = millis();

  // STARTボタン処理
  static int lastStart = HIGH;
  int start = digitalRead(SWITCH_START);
  if (lastStart == HIGH && start == LOW) {
    if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
    else startCount = 1;
    lastStartTime = now;

    if (startCount >= 2) {
      const uint8_t MSG_RESET[] = {MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T};
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
  lastStart = start;

  // フェーズ処理
  switch (phase) {
    case READY_PHASE:
      stopMotors();
      scrollReadyLoop();
      break;

    case COOL_PHASE:
      stopMotors(); // MOTOR1/MOTOR2を止める
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          phase = ACTIVE_PHASE;
          countdown = CYCLE_ACTIVE / 1000;
          cycle++;
          motor2Running = true;
          motor2Hits = 0;
          activateMagnet();
        }
      }
      showTimeCycle180(countdown, cycle);
      break;

    case ACTIVE_PHASE:
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          if (cycle >= MAX_CYCLES) {
            const uint8_t MSG_FINISH[] = {MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H};
            scrollMessage180(MSG_FINISH, 6, 300);
            resetSystem();
            break;
          }
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000;
          stopMotors();
        }
      }

      // MOTOR1スイッチ押下で10秒動作
      if (digitalRead(SWITCH1_PIN) == LOW) {
        motor1Active = true;
        motor1Start = now;
        digitalWrite(MOTOR1_RELAY_PIN, HIGH);
      }

      updateMotor1(now);
      driveMotor2();
      checkMotor2Piezo(now);
      updateMagnet(now);
      showTimeCycle180(countdown, cycle);
      break;
  }
}
