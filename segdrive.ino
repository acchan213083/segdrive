#include <TM1637Display.h>

// ==== Pin Config ====
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

// ==== Segment Definitions ====
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
const uint8_t MY_SEG_BLANK = 0x00;

// ==== Timing ====
const unsigned long MOTOR1_DURATION = 10000;
const unsigned long MOTOR2_PWM = 255;
const unsigned long PIEZO_DEBOUNCE = 100;
const int MOTOR2_STOP_TARGET = 5;
const int MAX_CYCLES = 10;
const unsigned long CYCLE_ACTIVE = 30000;
const unsigned long CYCLE_INTERVAL = 5000;

// ==== State ====
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

// ==== Scroll ====
unsigned long lastReadyScroll = 0;
int readyScrollIndex = -4;
const uint8_t MSG_READY[] = { MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y };

// ==== Start Button ====
unsigned long lastStartTime = 0;
int startCount = 0;
const unsigned long DOUBLE_PRESS_INTERVAL = 500;

// ==== Rotate ====
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

// ==== Display ====
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

// ==== Motors ====
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

void updateMotor1(unsigned long now) {
  Serial.print("updateMotor1: phase=");
  Serial.print(phase);
  Serial.print(", motor1Active=");
  Serial.println(motor1Active);

  if (motor1Active && (now - motor1Start >= MOTOR1_DURATION)) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
    Serial.println("Motor1 auto-stopped after 10s");
  }
  if (phase != ACTIVE_PHASE && motor1Active) {
    motor1Active = false;
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
    Serial.println("Motor1 stopped due to phase change");
  }
  if (motor1Active) {
    digitalWrite(MOTOR1_RELAY_PIN, HIGH);
    Serial.println("Motor1 relay set to HIGH");
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
  if (phase != ACTIVE_PHASE) {
    lastPiezoState = digitalRead(SWITCH2_PIN);
    return;
  }
  int p = digitalRead(SWITCH2_PIN);
  if (lastPiezoState == HIGH && p == LOW && (now - lastPiezo) > PIEZO_DEBOUNCE) {
    motor2Hits++;
    lastPiezo = now;
    if (motor2Hits >= MOTOR2_STOP_TARGET) motor2Running = false;
  }
  lastPiezoState = p;
}

// ==== Magnet ====
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW);
  magnetActive = true;
  magnetStart = millis();
  Serial.println("Electromagnet activated");
}

void updateMagnet(unsigned long now) {
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH);
    magnetActive = false;
    Serial.println("Electromagnet deactivated");
  }
}

// ==== Scroll ====
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

// ==== System ====
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

// ==== Setup ====
void setup() {
  Serial.begin(9600); // シリアル通信初期化

  // 入力ピンの設定
  pinMode(SWITCH_START, INPUT_PULLUP);   // スタートボタン
  pinMode(SWITCH1_PIN, INPUT_PULLUP);    // モーター1スイッチ
  pinMode(SWITCH2_PIN, INPUT);           // モーター2ピエゾセンサー

  // 出力ピンの設定
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);     // モーター1リレー
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);     // モーター2リレー
  pinMode(MOTOR2_IN1, OUTPUT);           // モーター2制御ピン1
  pinMode(MOTOR2_IN2, OUTPUT);           // モーター2制御ピン2
  pinMode(ELECTROMAGNET_PIN, OUTPUT);    // 電磁石
  pinMode(LED, OUTPUT);                  // LEDインジケーター

  // 初期状態の設定
  stopMotors();                          // すべてのモーター停止
  digitalWrite(ELECTROMAGNET_PIN, HIGH); // 電磁石OFF（HIGHでOFF）
  display.setBrightness(7);              // 7セグメントディスプレイの明るさ最大
  resetSystem();                         // READY状態に初期化
}

// ==== Main Loop ====
void loop() {
  unsigned long now = millis();

  // --- Start button handling ---
  static int lastStart = HIGH;
  int start = digitalRead(SWITCH_START);
  if (lastStart == HIGH && start == LOW) {
    if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
    else startCount = 1;
    lastStartTime = now;

    if (startCount >= 2) {
      stopMotors();
      const uint8_t MSG_RESET[] = { MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T };
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

  // --- Phase processing ---
  Serial.print("Current phase: ");
  Serial.println(phase);

  switch (phase) {
    case READY_PHASE:
      stopMotors();
      scrollReadyLoop();
      break;

    case COOL_PHASE:
      stopMotors();
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

          if (digitalRead(SWITCH1_PIN) == LOW) {
            motor1Active = true;
            motor1Start = now;
            digitalWrite(MOTOR1_RELAY_PIN, HIGH);
            Serial.println("Motor1 activated immediately after ACTIVE_PHASE started");
          }
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
            stopMotors();
            const uint8_t MSG_FINISH[] = { MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H };
            scrollMessage180(MSG_FINISH, 6, 300);
            resetSystem();
            break;
          }
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000;
          stopMotors();
        }
      }

      // ==== Motor1 switch handling ====
      static int lastS1 = HIGH;
      int s1 = digitalRead(SWITCH1_PIN);

      if (s1 != lastS1) {
        Serial.print("Motor1 switch changed: ");
        Serial.println(s1 == LOW ? "PRESSED" : "RELEASED");
        lastS1 = s1;
      }

      if (phase == ACTIVE_PHASE && s1 == LOW) {
        motor1Start = now;
        if (!motor1Active) {
          motor1Active = true;
          Serial.println("Motor1 started");
        } else {
          Serial.println("Motor1 timer reset");
        }
        digitalWrite(MOTOR1_RELAY_PIN, HIGH);
      }

      // ==== Motor updates ====
      updateMotor1(now);
      driveMotor2();
      checkMotor2Piezo(now);
      updateMagnet(now);
      showTimeCycle180(countdown, cycle);
      break;
  }
}
