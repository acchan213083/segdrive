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

TM1637Display display(CLK, DIO);

// 設定値
const unsigned long MAGNET_ON_DURATION = 5000; // 電磁石のON時間（ミリ秒）

bool state = false;
int t = 5;
int cycle = 0;
float speedFactor = 1.0;

unsigned long lastTick = 0;

bool magnet1Active = false;
bool magnet2Active = false;
unsigned long magnet1Start = 0;
unsigned long magnet2Start = 0;

int motorPower1 = 255;
int motorPower2 = 255;

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

  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

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

  drive();
  updateMagnets(now);
}

void drive() {
  if (state) {
    analogWrite(IN1, motorPower1);
    analogWrite(IN2, 0);
    analogWrite(IN3, motorPower2);
    analogWrite(IN4, 0);
    digitalWrite(LED, HIGH);
  } else {
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
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

  // モータと電磁石を停止
  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);
  digitalWrite(LED, LOW);
  digitalWrite(electromagnet1, LOW);
  digitalWrite(electromagnet2, LOW);

  // 30秒待機してディスプレイを消灯
  delay(30000); // 30秒（30000ミリ秒）
  display.clear(); // ディスプレイ消灯
}
