#include <TM1637Display.h>

#define motor1 4
#define motor2 5
#define CLK 2
#define DIO 3
#define LED 13

TM1637Display display(CLK, DIO);

bool state = false;
int t = 5;
int cycle = 0;
float speedFactor = .0; // 倍速係数

// セグメントコード定義（文字列 "FINISH" 用）
const uint8_t CHAR_F     = 0b01110001;
const uint8_t CHAR_I     = 0b00010000;
const uint8_t CHAR_N     = 0b01010100;
const uint8_t CHAR_S     = 0b01101101;
const uint8_t CHAR_H     = 0b01110100;
const uint8_t CHAR_BLANK = 0x00;

void setup() {
  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);
  pinMode(LED, OUTPUT);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
}

void loop() {
  drive();
  showTimeAndCycle(t, cycle);

  delay((int)(1000 / speedFactor));
  t -= 1;

  if (t == 0) {
    if (state == true) {
      if (cycle >= 10) {
        finishScroll(); // ← スクロール表示に変更
        while (true);
      } else {
        state = false;
        t = 5;
      }
    } else {
      cycle += 1;
      state = true;
      t = 45;
    }
  }
}

void drive() {
  digitalWrite(motor1, state ? HIGH : LOW);
  digitalWrite(motor2, state ? HIGH : LOW);
  digitalWrite(LED,    state ? HIGH : LOW);
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

void finishScroll() {
  // "FINISH" を右端から左へスクロール表示
  const uint8_t message[] = {
    CHAR_F, CHAR_I, CHAR_N, CHAR_I, CHAR_S, CHAR_H
  };
  const int len = sizeof(message);

  // スクロール表示（右端から左へ）
  for (int i = 0; i <= len + 3; i++) {
    uint8_t frame[4] = {
      (i >= 3 && i - 3 < len) ? message[i - 3] : CHAR_BLANK,
      (i >= 2 && i - 2 < len) ? message[i - 2] : CHAR_BLANK,
      (i >= 1 && i - 1 < len) ? message[i - 1] : CHAR_BLANK,
      (i < len)               ? message[i]     : CHAR_BLANK
    };
    display.setSegments(frame);
    delay(300); // スクロール速度
  }

  // 最後に "FIN " を表示して停止
  uint8_t final[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_BLANK };
  display.setSegments(final);

  digitalWrite(motor1, LOW);
  digitalWrite(motor2, LOW);
  digitalWrite(LED, LOW);
}
