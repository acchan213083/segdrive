/*
MIT License

Copyright © 2025 Namabayashi

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

#include <TM1637Display.h> // TM1637 4桁7セグメントLEDディスプレイを制御するライブラリを読み込む

// ==== ピン設定 ====
// Arduinoボードに接続された各ハードウェアのピン番号を定義
#define CLK 4                  // TM1637ディスプレイのクロックピン
#define DIO 5                  // TM1637ディスプレイのデータピン
#define SWITCH_START 6         // スタートボタンの入力ピン（LOWで押下）
#define SWITCH1_PIN 7          // Motor1制御スイッチの入力ピン（LOWで押下）
#define SWITCH2_PIN 8          // Motor2用の圧電センサー入力ピン（LOWパルスでヒット検出）
#define MOTOR1_RELAY_PIN 9     // Motor1のリレー制御出力ピン（HIGHでON）
#define MOTOR2_RELAY_PIN 10    // Motor2のリレー制御出力ピン（HIGHでON）
#define MOTOR2_IN1 11          // Motor2の方向制御ピン1（PWM可能）
#define MOTOR2_IN2 12          // Motor2の方向制御ピン2（PWM可能）
#define ELECTROMAGNET_PIN 2    // 電磁石の制御出力ピン（LOWでON）
#define LED 13                 // LEDインジケータの出力ピン（HIGHで点灯）

TM1637Display display(CLK, DIO); // CLKとDIOピンを使ってディスプレイオブジェクトを初期化

// ==== カスタム文字のセグメント定義 ====
// ディスプレイにスクロール表示する文字のセグメントパターンを定義
const uint8_t MY_SEG_R = 0b01010000; // 'R'のセグメントパターン
const uint8_t MY_SEG_E = 0b01111001; // 'E'のセグメントパターン
const uint8_t MY_SEG_A = 0b01110111; // 'A'のセグメントパターン
const uint8_t MY_SEG_D = 0b01011110; // 'D'のセグメントパターン
const uint8_t MY_SEG_Y = 0b01101110; // 'Y'のセグメントパターン
const uint8_t MY_SEG_F = 0b01110001; // 'F'のセグメントパターン
const uint8_t MY_SEG_I = 0b00000110; // 'I'のセグメントパターン
const uint8_t MY_SEG_N = 0b01010100; // 'N'のセグメントパターン
const uint8_t MY_SEG_S = 0b01101101; // 'S'のセグメントパターン
const uint8_t MY_SEG_H = 0b01110110; // 'H'のセグメントパターン
const uint8_t MY_SEG_T = 0b01111000; // 'T'のセグメントパターン
const uint8_t MY_SEG_BLANK = 0x00;   // 空白表示用のセグメント（すべてOFF）

// ==== タイミング定数 ====
// モーターやフェーズの動作時間・しきい値を定義
const unsigned long MOTOR1_DURATION = 10000; // Motor1の動作時間（10秒）
const unsigned long MOTOR2_PWM = 255;        // Motor2のPWM出力（最大速度）
const unsigned long PIEZO_DEBOUNCE = 80;     // 圧電センサーのデバウンス時間（ms）
const int MOTOR2_STOP_TARGET = 5;            // Motor2が停止するヒット数のしきい値
const int MAX_CYCLES = 10;                   // 最大サイクル数（10回で終了）
const unsigned long CYCLE_ACTIVE = 30000;    // ACTIVEフェーズの時間（30秒）
const unsigned long CYCLE_INTERVAL = 5000;   // COOLフェーズの休止時間（5秒）

// ==== システム状態変数 ====
// モーター、センサー、タイマー、フェーズの現在の状態を管理する変数群
unsigned long activePhaseStart = 0;          // ACTIVEフェーズが開始された時刻
bool motor1Active = false;                   // Motor1が現在動作中かどうか
unsigned long motor1Start = 0;               // Motor1が起動された時刻
bool motor2Running = false;                  // Motor2が現在動作中かどうか
int motor2Hits = 0;                          // ACTIVEフェーズ中の圧電センサーのヒット数
unsigned long lastPiezo = 0;                 // 最後に圧電センサーが反応した時刻
int lastPiezoState = HIGH;                   // 圧電センサーの前回の状態（HIGHまたはLOW）
unsigned long lastTick = 0;                  // カウントダウンの最後の更新時刻
int countdown = 0;                           // 残り時間（秒単位）
int cycle = 0;                               // 現在のサイクル番号（0〜MAX_CYCLES）
enum Phase { READY_PHASE, COOL_PHASE, ACTIVE_PHASE }; // フェーズを表す列挙型
Phase phase = READY_PHASE;                   // 初期フェーズはREADY
bool magnetActive = false;                   // 電磁石が現在ONかどうか
unsigned long magnetStart = 0;               // 電磁石がONになった時刻
bool motor1AutoTriggered = false;            // Motor1が自動起動されたかどうかのフラグ

// ==== READYメッセージスクロール用変数 ====
// ディスプレイに"READY"をスクロール表示するための制御変数
unsigned long lastReadyScroll = 0;           // 最後にスクロール更新した時刻
int readyScrollIndex = -4;                   // スクロール位置（初期は画面外）
const uint8_t MSG_READY[] = { MY_SEG_R, MY_SEG_E, MY_SEG_A, MY_SEG_D, MY_SEG_Y }; // "READY"のセグメント配列

// ==== スタートボタン検出用変数 ====
// スタートボタンのシングル・ダブルプレスを検出するための変数
unsigned long lastStartTime = 0;             // 最後にスタートボタンが押された時刻
int startCount = 0;                          // 検出された押下回数
const unsigned long DOUBLE_PRESS_INTERVAL = 500; // ダブルプレス判定の最大間隔（ms）

// ==== セグメント反転関数（180度回転） ====
// セグメントパターンを上下反転して表示方向を調整する関数
uint8_t rotate180(uint8_t seg) {
  uint8_t r = 0;
  if (seg & 0b00000001) r |= 0b00001000;     // 下部セグメント → 左上
  if (seg & 0b00000010) r |= 0b00010000;     // 左下 → 右上
  if (seg & 0b00000100) r |= 0b00100000;     // 右下 → 中央
  if (seg & 0b00001000) r |= 0b00000001;     // 左上 → 下部
  if (seg & 0b00010000) r |= 0b00000010;     // 右上 → 左下
  if (seg & 0b00100000) r |= 0b00000100;     // 中央 → 右下
  if (seg & 0b01000000) r |= 0b01000000;     // 中央縦セグメントはそのまま
  return r;                                  // 反転後のセグメントパターンを返す
}


// ==== 時間とサイクル数の表示（コロン付き） ====
// カウントダウン時間とサイクル番号を7セグメントディスプレイに表示する
// 圧電センサーのヒット数に応じてコロン（点滅や点灯）を追加する
void showTimeCycleWithHits(int sec, int cyc, int hits, unsigned long now) {
  static bool colonBlinkState = false;       // コロンの点滅状態を保持するフラグ
  static unsigned long lastBlinkTime = 0;    // 最後にコロンを点滅させた時刻
  const uint8_t COLON_SEGMENT = 0b10000000;  // コロン表示用のビットマスク

  // 秒数とサイクル数から各桁の数字を抽出
  int s1 = (sec / 10) % 10;                  // 秒の十の位
  int s0 = sec % 10;                         // 秒の一の位
  int c1 = (cyc / 10) % 10;                  // サイクルの十の位
  int c0 = cyc % 10;                         // サイクルの一の位

  // 数字をエンコードして上下反転させ、表示用の配列に格納
  uint8_t d[4] = {
    rotate180(display.encodeDigit(c0)),      // 一番右：サイクルの一の位
    rotate180(display.encodeDigit(c1)),      // 次：サイクルの十の位
    rotate180(display.encodeDigit(s0)),      // 次：秒の一の位
    rotate180(display.encodeDigit(s1))       // 一番左：秒の十の位
  };

  // ヒット数に応じてコロンを追加
  switch (hits) {
    case 1:
      // ヒットが1回 → サイクルと時間の両方にコロンを表示
      d[1] |= COLON_SEGMENT; // サイクル側にコロン追加
      d[2] |= COLON_SEGMENT; // 時間側にコロン追加
      break;
    case 2:
      // ヒットが2回 → サイクル側のみコロン表示
      d[1] |= COLON_SEGMENT;
      break;
    case 3:
      // ヒットが3回 → 時間側のみコロン表示
      d[2] |= COLON_SEGMENT;
      break;
    case 4:
      // ヒットが4回 → サイクルと時間のコロンを交互に点滅
      if (now - lastBlinkTime >= 500) { // 最後の点滅から500ms経過したか確認
        colonBlinkState = !colonBlinkState; // 点滅状態を反転
        lastBlinkTime = now; // 点滅時刻を更新
      }
      if (colonBlinkState)
        d[1] |= COLON_SEGMENT; // サイクル側にコロン表示
      else
        d[2] |= COLON_SEGMENT; // 時間側にコロン表示
      break;
  }

  display.setSegments(d); // 最終的な表示データをディスプレイに送信
}

// ==== 全モーター停止処理 ====
// すべてのモーターを停止し、関連するフラグをリセットする
void stopMotors() {
  motor1Active = false; // Motor1の動作フラグをOFF
  motor2Running = false; // Motor2の動作フラグをOFF
  motor2Hits = 0; // Motor2のヒットカウントをリセット
  digitalWrite(MOTOR1_RELAY_PIN, LOW); // Motor1のリレーをOFF
  digitalWrite(MOTOR2_RELAY_PIN, LOW); // Motor2のリレーをOFF
  analogWrite(MOTOR2_IN1, 0); // Motor2のPWM出力を停止（IN1）
  analogWrite(MOTOR2_IN2, 0); // Motor2のPWM出力を停止（IN2）
  digitalWrite(LED, LOW); // LEDインジケータをOFF
}

// ==== Motor1制御処理 ====
// Motor1の動作時間とフェーズに応じて停止処理を行う
void updateMotor1(unsigned long now) {
  // Motor1が動作中で、設定時間を超えた場合は停止
  if (motor1Active && (now - motor1Start >= MOTOR1_DURATION)) {
    motor1Active = false; // Motor1の動作フラグをOFF
    digitalWrite(MOTOR1_RELAY_PIN, LOW); // Motor1のリレーをOFF
    // Serial.println("Motor1 stopped after 10s"); // デバッグ用出力（任意）
  }

  // フェーズがACTIVE以外でMotor1が動作中なら強制停止
  if (phase != ACTIVE_PHASE && motor1Active) {
    motor1Active = false; // Motor1の動作フラグをOFF
    digitalWrite(MOTOR1_RELAY_PIN, LOW); // Motor1のリレーをOFF
    // Serial.println("Motor1 stopped due to phase change"); // デバッグ用出力（任意）
  }

  // Motor1が動作中ならリレーをONに維持
  if (motor1Active) {
    digitalWrite(MOTOR1_RELAY_PIN, HIGH); // Motor1のリレーをON
    // Serial.println("Motor1 running..."); // デバッグ用出力（任意）
  }
}


// ==== Motor2駆動処理 ====
// ACTIVEフェーズ中にMotor2を制御する
// PWMで前進させ、リレーとLEDをONにする
// 動作していない場合はモーターと表示を停止する
void driveMotor2() {
  // Motor2が動作中かつフェーズがACTIVEなら前進動作
  if (motor2Running && phase == ACTIVE_PHASE) {
    analogWrite(MOTOR2_IN1, MOTOR2_PWM); // IN1にPWM出力（最大速度）
    analogWrite(MOTOR2_IN2, 0);          // IN2は0で前進方向
    digitalWrite(MOTOR2_RELAY_PIN, HIGH); // Motor2のリレーをON
    digitalWrite(LED, HIGH);              // LEDインジケータをON
  } else {
    // Motor2が停止中、またはフェーズがACTIVEでない場合は停止
    analogWrite(MOTOR2_IN1, 0);           // IN1のPWM出力を停止
    analogWrite(MOTOR2_IN2, 0);           // IN2のPWM出力を停止
    digitalWrite(MOTOR2_RELAY_PIN, LOW);  // Motor2のリレーをOFF
    digitalWrite(LED, LOW);               // LEDインジケータをOFF
  }
}

// ==== Motor2用圧電センサーのヒット検出 ====
// ACTIVEフェーズ中に圧電センサーのヒットを検出する
// ヒット数をカウントし、目標数に達したらMotor2を停止する
void checkMotor2Piezo(unsigned long now) {
  // ACTIVEフェーズ以外ではセンサー状態を更新して終了
  if (phase != ACTIVE_PHASE) {
    lastPiezoState = digitalRead(SWITCH2_PIN); // 現在のセンサー状態を読み取る
    return;
  }

  int p = digitalRead(SWITCH2_PIN); // センサーの現在の入力状態を取得

  // HIGH→LOWの変化（立下り）を検出し、デバウンス処理を行う
  if (lastPiezoState == HIGH && p == LOW && (now - lastPiezo) > PIEZO_DEBOUNCE) {
    motor2Hits++;       // ヒットカウントを1増加
    lastPiezo = now;    // 最後のヒット時刻を更新
    Serial.print("Piezo hit count: "); // デバッグ出力
    Serial.println(motor2Hits);        // 現在のヒット数を表示

    // ヒット数が目標に達したらMotor2を停止
    if (motor2Hits >= MOTOR2_STOP_TARGET)
      motor2Running = false;
  }

  lastPiezoState = p; // 次回の検出のために状態を保存
}

// ==== 電磁石の起動処理 ====
// 電磁石をONにし、起動時刻を記録する
void activateMagnet() {
  digitalWrite(ELECTROMAGNET_PIN, LOW); // 電磁石をON（LOWで起動）
  magnetActive = true;                  // 電磁石の状態をONに設定
  magnetStart = millis();               // 起動時刻を記録
}

// ==== 電磁石の自動停止処理（10秒後） ====
// 電磁石がONになってから10秒経過したらOFFにする
void updateMagnet(unsigned long now) {
  // 電磁石がONかつ10秒経過していたらOFFにする
  if (magnetActive && now - magnetStart >= 10000) {
    digitalWrite(ELECTROMAGNET_PIN, HIGH); // 電磁石をOFF（HIGHで停止）
    magnetActive = false;                  // 状態をOFFに更新
  }
}

// ==== "READY"メッセージのスクロール表示 ====
// ディスプレイに"READY"という文字列をスクロール表示する
void scrollReadyLoop() {
  unsigned long now = millis(); // 現在時刻を取得

  // 200msごとにスクロールを更新
  if (now - lastReadyScroll >= 200) {
    lastReadyScroll = now; // 最終更新時刻を記録
    const int len = 5;     // "READY"の文字数
    uint8_t frame[4];      // 4桁分の表示バッファ

    // 表示バッファに文字または空白を格納
    for (int i = 0; i < 4; i++) {
      int idx = readyScrollIndex - i;
      frame[i] = (idx >= 0 && idx < len) ? rotate180(MSG_READY[idx]) : MY_SEG_BLANK;
    }

    display.setSegments(frame); // ディスプレイに表示
    readyScrollIndex++;         // スクロール位置を進める

    // メッセージが通過したらスクロール位置をリセット
    if (readyScrollIndex > (len + 3)) readyScrollIndex = -4;
  }
}

// ==== 任意メッセージのスクロール表示 ====
// 任意の文字列をディスプレイにスクロール表示する（180度反転付き）
void scrollMessage180(const uint8_t* msg, int len, int delayMs) {
  // メッセージを右から左へスクロール表示
  for (int i = len + 3; i >= 0; i--) {
    uint8_t frame[4] = { 0 }; // 表示バッファを初期化

    // 表示バッファに文字または空白を格納
    for (int j = 0; j < 4; j++) {
      int idx = i - j;
      frame[3 - j] = (idx >= 0 && idx < len) ? rotate180(msg[len - idx - 1]) : MY_SEG_BLANK;
    }

    display.setSegments(frame); // ディスプレイに表示
    delay(delayMs);             // 次のフレームまで待機
  }
}

// ==== システムの初期化（READY状態へ） ====
// カウンターや状態をリセットし、READYフェーズに戻す
void resetSystem() {
  countdown = 0;                     // カウントダウンタイマーをリセット
  cycle = 0;                         // サイクルカウンターをリセット
  phase = READY_PHASE;              // フェーズをREADYに設定
  stopMotors();                     // モーターをすべて停止
  magnetActive = false;             // 電磁石の状態をOFFに設定
  digitalWrite(ELECTROMAGNET_PIN, HIGH); // 電磁石をOFFにする
  lastTick = millis();              // タイマー更新時刻を記録
  readyScrollIndex = -4;            // スクロール位置を初期化
  showTimeCycleWithHits(countdown, cycle, motor2Hits, millis()); // ディスプレイを更新
}


// ==== Arduinoの初期設定 ====
// Arduinoが起動またはリセットされたときに1回だけ実行される関数
// ピンのモードを設定し、システムをREADY状態に初期化する
void setup() {
  Serial.begin(9600); // デバッグ用にシリアル通信を開始

  // 入力ピンの設定
  pinMode(SWITCH_START, INPUT_PULLUP); // スタートボタン（LOWで押下）
  pinMode(SWITCH1_PIN, INPUT_PULLUP);  // Motor1制御スイッチ（LOWで押下）
  pinMode(SWITCH2_PIN, INPUT);         // 圧電センサー入力（LOWパルスでヒット検出）

  // 出力ピンの設定
  pinMode(MOTOR1_RELAY_PIN, OUTPUT);   // Motor1のリレー制御ピン
  pinMode(MOTOR2_RELAY_PIN, OUTPUT);   // Motor2のリレー制御ピン
  pinMode(MOTOR2_IN1, OUTPUT);         // Motor2の方向制御ピン1（PWM対応）
  pinMode(MOTOR2_IN2, OUTPUT);         // Motor2の方向制御ピン2（PWM対応）
  pinMode(ELECTROMAGNET_PIN, OUTPUT);  // 電磁石の制御ピン
  pinMode(LED, OUTPUT);                // LEDインジケータの制御ピン

  // システム状態の初期化
  stopMotors();                        // すべてのモーターを停止
  digitalWrite(ELECTROMAGNET_PIN, HIGH); // 電磁石をOFF（HIGHで停止）
  display.setBrightness(7);            // ディスプレイの明るさを最大に設定
  resetSystem();                       // READYフェーズにリセット
}

// ==== メインループ ====
// setup()の後に繰り返し実行される関数
// ボタン入力、フェーズの切り替え、モーター制御、表示更新などを処理する
void loop() {
  unsigned long now = millis(); // 現在時刻（ミリ秒）を取得

  // --- スタートボタンのデバウンス処理 ---
  // 機械的なノイズによる誤検出を防ぐ
  const unsigned long DEBOUNCE_DELAY = 50; // デバウンス時間（ms）
  static int lastRawStart = HIGH;          // 前回の生のボタン状態
  static int stableStart = HIGH;           // 安定したボタン状態
  static unsigned long lastStartDebounce = 0; // 最後に状態が変化した時刻
  static bool startHandled = false;        // 同じ押下の重複処理を防ぐフラグ

  int rawStart = digitalRead(SWITCH_START); // 現在のボタン状態を読み取る
  if (rawStart != lastRawStart) {
    lastStartDebounce = now; // デバウンスタイマーをリセット
    startHandled = false;    // 新しい押下として処理可能にする
  }
  lastRawStart = rawStart; // 前回の状態を更新

  // デバウンス時間を超えて状態が安定しているか確認
  if ((now - lastStartDebounce) > DEBOUNCE_DELAY) {
    if (rawStart != stableStart) {
      stableStart = rawStart; // 安定状態を更新
      if (stableStart == LOW && !startHandled) {
        startHandled = true; // この押下を処理済みにする

        // --- シングル・ダブルプレスの判定 ---
        if (now - lastStartTime < DOUBLE_PRESS_INTERVAL) startCount++;
        else startCount = 1;
        lastStartTime = now;

        if (startCount >= 2) {
          // ダブルプレス：システムをリセット
          stopMotors(); // モーターをすべて停止
          const uint8_t MSG_RESET[] = { MY_SEG_R, MY_SEG_E, MY_SEG_S, MY_SEG_E, MY_SEG_T };
          scrollMessage180(MSG_RESET, 5, 200); // "RESET"メッセージをスクロール表示
          resetSystem(); // システムを初期化
          startCount = 0; // カウントをリセット
        } else {
          // シングルプレス：READYとCOOLフェーズを切り替え
          if (phase == READY_PHASE) {
            phase = COOL_PHASE; // COOLフェーズへ移行
            countdown = CYCLE_INTERVAL / 1000; // COOLフェーズのカウントダウン設定
            lastTick = now; // タイマー更新
            stopMotors(); // モーターを停止
          } else {
            phase = READY_PHASE; // READYフェーズに戻す
            countdown = 0;
            stopMotors();
            readyScrollIndex = -4; // スクロール位置を初期化
          }
        }
      }
    }
  }

  // --- フェーズ制御 ---
  switch (phase) {
    case READY_PHASE:
      stopMotors();       // モーターを停止
      scrollReadyLoop();  // "READY"メッセージをスクロール表示
      break;

    case COOL_PHASE:
      stopMotors();       // モーターを停止
      motor1AutoTriggered = false; // Motor1の自動起動フラグをリセット

      // COOLフェーズのカウントダウン処理
      if (now - lastTick >= 1000) {
        lastTick = now;
        countdown--;
        if (countdown <= 0) {
          // ACTIVEフェーズへ移行
          phase = ACTIVE_PHASE;
          countdown = CYCLE_ACTIVE / 1000; // ACTIVEフェーズのカウントダウン設定
          cycle++; // サイクル数をインクリメント
          motor2Running = true; // Motor2を起動
          motor2Hits = 0;       // ヒットカウントをリセット
          activateMagnet();     // 電磁石をON
          activePhaseStart = now; // ACTIVEフェーズの開始時刻を記録
        }
      }
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now); // ディスプレイを更新
      break;

        case ACTIVE_PHASE:
      // ==== ACTIVEフェーズ（メイン動作フェーズ） ====
      // モーターが動作し、圧電センサーのヒットをカウントするフェーズ

      // --- ACTIVEフェーズのカウントダウン処理 ---
      if (now - lastTick >= 1000) { // 1秒ごとにカウントダウンを更新
        lastTick = now;             // 最終更新時刻を記録
        countdown--;                // 残り時間を1秒減らす

        if (countdown <= 0) {       // カウントが0になったらフェーズ終了
          motor1AutoTriggered = false; // Motor1の自動起動フラグをリセット

          if (cycle >= MAX_CYCLES) { // 最大サイクル数に達した場合
            stopMotors(); // すべてのモーターを停止
            const uint8_t MSG_FINISH[] = { MY_SEG_F, MY_SEG_I, MY_SEG_N, MY_SEG_I, MY_SEG_S, MY_SEG_H };
            scrollMessage180(MSG_FINISH, 6, 300); // "FINISH"メッセージをスクロール表示
            resetSystem(); // システムを初期化してREADYフェーズに戻す
            break; // ループを抜ける
          }

          // 次のフェーズへ移行（COOLフェーズ）
          phase = COOL_PHASE;
          countdown = CYCLE_INTERVAL / 1000; // COOLフェーズの時間を設定
          stopMotors(); // モーターを停止
        }
      }

      // --- Motor1ボタンのデバウンス処理 ---
      const unsigned long DEBOUNCE_S1 = 50; // Motor1ボタンのデバウンス時間（ms）
      static int lastRawS1 = HIGH;          // 前回の生のボタン状態
      static unsigned long lastS1Debounce = 0; // 最後に状態が変化した時刻

      int rawS1 = digitalRead(SWITCH1_PIN); // Motor1ボタンの現在の状態を読み取る
      if (rawS1 != lastRawS1) {
        lastS1Debounce = now; // 状態が変化したらデバウンスタイマーをリセット
      }
      lastRawS1 = rawS1; // 前回の状態を更新

      // --- Motor1の手動起動処理 ---
      if (digitalRead(SWITCH1_PIN) == LOW) {
        motor1Start = now;     // Motor1の起動時刻を記録
        motor1Active = true;   // Motor1を起動
      }

      // --- Motor1の自動起動処理（残り20秒で1回だけ） ---
      if (!motor1Active && !motor1AutoTriggered && countdown == 20) {
        motor1Start = now;     // Motor1の起動時刻を記録
        motor1Active = true;   // Motor1を起動
        motor1AutoTriggered = true; // 同じサイクル内での再起動を防止
      }

      // --- Motor1の状態更新 ---
      updateMotor1(now); // 時間切れやフェーズ変更による停止処理を実行

      // --- Motor2の停止条件判定 ---
      // ヒット数が目標に達した、または残り時間が10秒以下になった場合
      if (motor2Running && (motor2Hits >= MOTOR2_STOP_TARGET || (now - activePhaseStart >= CYCLE_ACTIVE - 10000))) {
        motor2Running = false; // Motor2を停止
      }

      // --- Motor2およびその他のコンポーネントの更新処理 ---
      driveMotor2();          // Motor2のPWM制御とリレー・LEDの更新
      checkMotor2Piezo(now);  // 圧電センサーのヒット検出とカウント
      updateMagnet(now);      // 電磁石の自動停止（10秒後）
      showTimeCycleWithHits(countdown, cycle, motor2Hits, now); // ディスプレイ表示を更新
      break; // ACTIVEフェーズの処理終了
  }
}
