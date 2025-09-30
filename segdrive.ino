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

// Pin definitions
#define ELECTROMAGNET1_PIN 2     // Electromagnet 1 control
#define ELECTROMAGNET2_PIN 3     // Electromagnet 2 control
#define CLK 4                    // TM1637 clock pin
#define DIO 5                    // TM1637 data pin
#define SWITCH1_PIN 6            // Motor1 trigger switch
#define SWITCH2_PIN 7            // Motor2 stop counter switch
#define MOTOR1_RELAY_PIN 8       // Motor1 relay output
#define MOTOR2_RELAY_PIN 9       // Motor2 relay output
#define MOTOR2_IN1 10            // Motor2 PWM IN1
#define MOTOR2_IN2 11            // Motor2 PWM IN2
#define LED 13                   // Status LED

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

bool motor1Active = false;
unsigned long motor1StartTime = 0;

bool motor2Running = false;
int motor2StopCount = 0;

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

  pinMode(ELECTROMAGNET1_PIN, OUTPUT);
  pinMode(ELECTROMAGNET2_PIN, OUTPUT);
  pinMode(MOTOR2_IN1, OUTPUT);
  pinMode(MOTOR2_IN2, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(SWITCH1_PIN, INPUT_PULLUP);
  pinMode(SWITCH2_PIN, INPUT_PULLUP);
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);

  digitalWrite(ELECTROMAGNET1_PIN, LOW);
  digitalWrite(ELECTROMAGNET2_PIN, LOW);
  digitalWrite(MOTOR2_IN1, LOW);
  digitalWrite(MOTOR2_IN2, LOW);
  digitalWrite(LED, LOW);
  digitalWrite(MOTOR1_RELAY_PIN, LOW);
  digitalWrite(MOTOR2_RELAY_PIN, LOW);

  display.setBrightness(7);
  showTimeAndCycle(t, cycle);
  lastTick = millis();
}

void loop() {
  unsigned long now = millis();

  if (state && digitalRead(SWITCH1_PIN) == LOW) {
    motor1Active = true;
    motor1StartTime = now;
    digitalWrite(LED, HIGH);
    digitalWrite(MOTOR1_RELAY_PIN, HIGH);
    Serial.println("Motor1 Relay/LED ON for 10s");
  }

  if (motor1Active && (!state || now - motor1StartTime >= MOTOR_ON_DURATION)) {
    motor1Active = false;
    digitalWrite(LED, LOW);
    digitalWrite(MOTOR1_RELAY_PIN, LOW);
    Serial.println("Motor1 Relay/LED OFF");
  }

  if (motor2Running && digitalRead(SWITCH2_PIN) == LOW) {
    motor2StopCount++;
    Serial.print("Motor2 switch pressed: ");
    Serial.println(motor2StopCount);
    delay(300); // debounce

    if (motor2StopCount >= 5) {
      motor2Running = false;
      Serial.println("Motor2 stopped after 5 presses");
    }
  }

  if (motor1Active && now - lastRotate >= ROTATE_INTERVAL) {
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
          if (motor1Active) {
            motor1Active = false;
            digitalWrite(LED, LOW);
            digitalWrite(MOTOR1_RELAY_PIN, LOW);
            Serial.println("Motor1 Relay/LED OFF due to interval");
          }
        }
      } else {
        activateMagnet1();
        cycle += 1;
        state = true;
        t = 40;

        motor2Running = true;
        motor2StopCount = 0;
        Serial.println("Motor2 started at cycle begin");
      }
    }
  }

  driveSecondMotor();
  updateMagnets(now);
}

void driveSecondMotor() {
  if (motor2Running) {
    analogWrite(MOTOR2_IN1, motorPower2);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, HIGH);
  } else {
    analogWrite(MOTOR2_IN1, 0);
    analogWrite(MOTOR2_IN2, 0);
    digitalWrite(MOTOR2_RELAY_PIN, LOW);
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
  digitalWrite(ELECTROMAGNET1_PIN, HIGH);
  magnet1Active = true;
  magnet1Start = millis();
  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true);
}

void activateMagnet2() {
  Serial.println("Magnet2 ON");
  digitalWrite(ELECTROMAGNET2_PIN, HIGH);
  magnet2Active = true;
  magnet2Start = millis();
  int combined = t * 100 + cycle;
  display.showNumberDecEx(combined, 0x40, true);
}

void updateMagnets(unsigned long now) {
  if (magnet1Active && now - magnet1Start >= MAGNET_ON_DURATION) {
    Serial.println("Magnet1 OFF");
    digitalWrite(ELECTROMAGNET1_PIN, LOW);
    magnet1Active = false;
  }
  if (magnet2Active && now - magnet2Start >= MAGNET_ON_DURATION) {
    Serial.println("Magnet2 OFF");
    digitalWrite(ELECTROMAGNET2_PIN, LOW);
    magnet2Active = false;
  }
}

void finishScroll() {
  // Message to scroll: "FINISH"
  const uint8_t message[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_I, CHAR_S, CHAR_H };
  const int len = sizeof(message);

  // Scroll the message across the 4-digit display
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

  // Display final static message: "FIN "
  uint8_t final[] = { CHAR_F, CHAR_I, CHAR_N, CHAR_BLANK };
  display.setSegments(final);

  // Stop Motor2 PWM
  analogWrite(MOTOR2_IN1, 0);
  analogWrite(MOTOR2_IN2, 0);

  // Turn off all outputs
  digitalWrite(LED, LOW);
  digitalWrite(ELECTROMAGNET1_PIN, LOW);
  digitalWrite(ELECTROMAGNET2_PIN, LOW);
  digitalWrite(MOTOR1_RELAY_PIN, LOW);
  digitalWrite(MOTOR2_RELAY_PIN, LOW);

  // Wait 30 seconds before clearing display
  delay(30000);
  display.clear();
}
