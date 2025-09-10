/*
MIT License

Copyright (c) 2025 [Your Name]

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

#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7
#define CLK 2
#define DIO 3
#define LED 13
#define electromagnet1 8
#define electromagnet2 9

TM1637Display display(CLK, DIO);

bool state = false;
int t = 5;
int cycle = 0;
float speedFactor = 1.0;

unsigned long lastTick = 0;

bool magnet1Active = false;
bool magnet2Active = false;
unsigned long magnet1Start = 0;
unsigned long magnet2Start = 0;

// セグメントコード定義
const uint8_t CHAR_F     = 0b01110001;
const uint8_t CHAR_I     = 0b00010000;
const uint8_t CHAR_N     = 0b01010100;
const uint8_t CHAR_S     = 0b01101101;
const uint8_t CHAR_H     = 0b01110100;
const uint8_t CHAR_BLANK = 0x00;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(electromagnet1, OUTPUT);
  pinMode(electromagnet2, OUTPUT);

  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  // カウントダウン処理
  if (now - lastTick >= (unsigned long)(1000 / speedFactor)) {
    lastTick = now;
    t -= 1;
    showTimeAndCycle(t, cycle);

    if (t == 0) {
      if (state == true) {
        // 動作終了 → 電磁石2をON
        if (cycle >= 10) {
          finishScroll();
          while (true);
        } else {
          activateMagnet2();
          state = false;
          t = 5;
        }
      } else {
        // インターバル終了 → 電磁石1をON
        activateMagnet1();
        cycle += 1;
        state = true;
        t = 45;
      }
    }
  }

  drive();
  updateMagnets(now);
}

void drive() {
  if (state) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
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
  digitalWrite(electromagnet1, HIGH);
  magnet1Active = true;
  magnet1Start = millis();

  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true); // コロンON
}

void activateMagnet2() {
  digitalWrite(electromagnet2, HIGH);
  magnet2Active = true;
  magnet2Start = millis();

  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true); // コロンON
}

void updateMagnets(unsigned long now) {
  if (magnet1Active && now - magnet1Start >= (unsigned long)(1000 / speedFactor)) {
    showTimeAndCycle(t, cycle); // コロンOFF
  }
  if (magnet2Active && now - magnet2Start >= (unsigned long)(1000 / speedFactor)) {
    showTimeAndCycle(t, cycle); // コロンOFF
  }

  if (magnet1Active && now - magnet1Start >= (unsigned long)(5000 / speedFactor)) {
    digitalWrite(electromagnet1, LOW);
    magnet1Active = false;
  }
  if (magnet2Active && now - magnet2Start >= (unsigned long)(5000 / speedFactor)) {
    digitalWrite(electromagnet2, LOW);
    magnet2Active = false;
  }
}

void finishScroll() {
  const uint8_t message[] = {
    CHAR_F, CHAR_I, CHAR_N, CHAR_I, CHAR_S, CHAR_H
  };
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

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);
}
