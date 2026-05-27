#if defined(ARDUINO)
  #include <Arduino.h>
  #define HAS_ARDUINO_HEADER 1
#elif defined(__has_include)
  #if __has_include(<Arduino.h>)
    #include <Arduino.h>
    #define HAS_ARDUINO_HEADER 1
  #elif __has_include(<arduino.h>)
    #include <arduino.h>
    #define HAS_ARDUINO_HEADER 1
  #elif __has_include("Arduino.h")
    #include "Arduino.h"
    #define HAS_ARDUINO_HEADER 1
  #endif
#endif

#if !defined(HAS_ARDUINO_HEADER)
  #include <stdint.h>
  extern unsigned long millis(void);
  extern int digitalRead(uint8_t pin);
  extern void digitalWrite(uint8_t pin, uint8_t value);
  extern void analogWrite(uint8_t pin, int value);
  extern void pinMode(uint8_t pin, uint8_t mode);
  #ifndef LOW
    #define LOW 0
  #endif
  #ifndef HIGH
    #define HIGH 1
  #endif
  #ifndef OUTPUT
    #define OUTPUT 1
  #endif
  #ifndef INPUT_PULLUP
    #define INPUT_PULLUP 2
  #endif
#endif



//ピン定義
const int PIN_BUTTON     = 2;       // タクトスイッチ（INPUT_PULLUP）
const int PIN_LED_GREEN  = 9;       // 緑LED（電源ON表示）
const int PIN_LED_YELLOW = 10;      // リマインド/フィードバックLED
const int PIN_BUZZER     = 12;       // アクティブブザー

//状態定義
const int STATE_IDLE    = 0;     // 待機中
const int STATE_REMIND  = 1;     // リマインド中
const int STATE_WARNING = 2;     // 警告中

//状態管理変数
int currentState = STATE_IDLE;

//時間定数
// unsigned long remindInterval = 10000;   // 待機中からリマインド（テスト）

unsigned long       remindInterval      = 1800000;       // リマインド通知までの待機時間（初期値30分）
const unsigned long warningDelay        = 30000;         // リマインド開始後、警告状態へ移行するまでの猶予時間
unsigned long       alertRepeatInterval = 10000;         // 警告中にブザーを再鳴動する間隔
const unsigned long feedbackDuration    = 1000;          // 待機中ボタン押下時の黄色LEDフィードバック表示時間
const unsigned long debounceDelay       = 50;            // ボタンのチャタリングを無効化する判定時間
const unsigned long longPressThreshold  = 2000;          // 長押しとして扱う最小押下時間　※任意

const unsigned long feedbackBlinkInterval = 250;         // 点滅反転間隔（見やすさ重視）
const unsigned int remindLight          = 200;           // リマインド中に黄色LEDの光る数
const unsigned long buzzerOnDuration    = 120;           // ブザー1回の鳴動時間
const unsigned long buzzerGapDuration   = 120;           // 開始時2回鳴動の間隔

//入力状態
bool buttonState        = false;   // 現在の確定状態（押下=true）
bool prevButtonReading  = false;   // 前回読取値
bool isLongPressHandled = false;   // 同一長押しの重複処理防止

// タイマー関連
unsigned long lastRemindMillis       = 0;   // 最後にリマインド時刻を更新した時刻
unsigned long warningStartMillis     = 0;   // リマインド開始時刻（30秒経過判定基準）
unsigned long lastAlertMillis        = 0;   // 最後にブザーを鳴らした時刻
unsigned long lastButtonMillis       = 0;   // デバウンス確定時刻
unsigned long feedbackStartMillis    = 0;   // 黄色LEDフィードバック開始時刻
unsigned long buttonPressStartMillis = 0;   // 長押し開始時刻　※任意

unsigned long lastFeedbackBlinkMillis = 0;  // 点滅を最後に反転した時刻

//フィードバック・通知状態
bool isFeedbackActive = false;    // 待機中ボタン押下の黄色LED点滅中か
int currentPattern    = 0;        // 通知パターン番号（0:標準）
int intervalModeIndex = 0;        // 0:30分 1:45分 2:60分　※任意

bool feedbackLedOn    = false;    // 点滅中のON/OFF状態

//その他のフラグ・カウンター
int buzzerPulseCount = 0;        // 開始時2回鳴動の回数管理



//-----------------------------関数-----------------------------
//ボタン押下イベント関数
bool readButton() {
  bool raw = (digitalRead(PIN_BUTTON) == LOW);  // LOW=押下
  unsigned long now = millis();

  // 生入力が変化した時刻を記録
  if (raw != prevButtonReading) {
    lastButtonMillis = now;
  }

  bool pressedEvent = false;

  // 変化後、一定時間経過したら確定
  if ((now - lastButtonMillis) >= debounceDelay) {
    if (raw != buttonState) {
      buttonState = raw;

      // false -> true の瞬間だけイベント発生
      if (buttonState) {
        pressedEvent = true;
      }
    }
  }

  prevButtonReading = raw;
  return pressedEvent;
}


//状態に応じてLED更新関数
void updateStatusLeds() {
  // 緑LEDは常時点灯
  digitalWrite(PIN_LED_GREEN, HIGH);

  // 黄色LED制御
  if (currentState == STATE_REMIND || currentState == STATE_WARNING) {
    // リマインド中は点灯
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    // 待機中は通常消灯（フィードバック中のみ点滅）
    if (isFeedbackActive) {
      unsigned long now = millis();
      if (now - lastFeedbackBlinkMillis >= feedbackBlinkInterval) {
        lastFeedbackBlinkMillis = now;
        feedbackLedOn = !feedbackLedOn;
      }
      analogWrite(PIN_LED_YELLOW, feedbackLedOn ? remindLight : 0);
    } else {
      digitalWrite(PIN_LED_YELLOW, LOW);
    }
  }
}


//ボタン押下で待機復帰
void resetByButton(bool pressedEvent) {
  if (!pressedEvent) return;

  unsigned long now = millis();

  // どの状態でも待機へ戻す
  currentState = STATE_IDLE;

  // リマインド起点をリセット
  lastRemindMillis = now;
  warningStartMillis = 0;
  lastAlertMillis = 0;
  buzzerPulseCount = 0;
  digitalWrite(PIN_BUZZER, LOW);

  // 1秒フィードバック開始
  isFeedbackActive = true;
  feedbackStartMillis = now;
  lastFeedbackBlinkMillis = now;
  feedbackLedOn = false;
  analogWrite(PIN_LED_YELLOW, 0);
}


//待機中にリマインド間隔の経過を判定する
void checkRemindTimer(){
  if (currentState != STATE_IDLE) return;

  if (remindInterval == 0) {
    remindInterval = 1800000;
  }

  unsigned long now = millis();
  if ((now - lastRemindMillis) >= remindInterval) {
    currentState = STATE_REMIND;
    warningStartMillis = now;
    lastAlertMillis = 0;
    buzzerPulseCount = 0;
    digitalWrite(PIN_BUZZER, LOW);
  }
}


//リマインド中30秒無反応で警告遷移し、再鳴動を管理する
void checkWarningTimeout(){
  static bool sequenceActive = false;
  static bool buzzerOn = false;
  static unsigned long phaseStartMillis = 0;

  unsigned long now = millis();

  if (currentState == STATE_REMIND) {
    if (warningStartMillis == 0) {
      warningStartMillis = now;
    }

    if ((now - warningStartMillis) >= warningDelay) {
      currentState = STATE_WARNING;
      buzzerPulseCount = 0;
      lastAlertMillis = now;
      sequenceActive = true;
      buzzerOn = false;
      phaseStartMillis = now;
      digitalWrite(PIN_BUZZER, LOW);
    }
    return;
  }

  if (currentState != STATE_WARNING) {
    sequenceActive = false;
    buzzerOn = false;
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }

  // 警告開始時2回鳴動（non-blocking）
  if (buzzerPulseCount < 2) {
    if (!buzzerOn) {
      if ((buzzerPulseCount == 0) || ((now - phaseStartMillis) >= buzzerGapDuration)) {
        digitalWrite(PIN_BUZZER, HIGH);
        buzzerOn = true;
        phaseStartMillis = now;
      }
    } else if ((now - phaseStartMillis) >= buzzerOnDuration) {
      digitalWrite(PIN_BUZZER, LOW);
      buzzerOn = false;
      phaseStartMillis = now;
      buzzerPulseCount++;
      if (buzzerPulseCount >= 2) {
        sequenceActive = false;
        lastAlertMillis = now;
      }
    }
    return;
  }

  // 2回鳴動後は一定間隔で「毎回2回」再鳴動
  if (!sequenceActive && (now - lastAlertMillis) >= alertRepeatInterval) {
    sequenceActive = true;
    buzzerOn = false;
    buzzerPulseCount = 0;
    phaseStartMillis = now;
    digitalWrite(PIN_BUZZER, LOW);
  }

}
//--------------------------------------------------------------


void setup() {
  //====================電源LED関係==========================
  pinMode(PIN_LED_GREEN, OUTPUT);     // 緑LEDを出力に設定
  digitalWrite(PIN_LED_GREEN, HIGH);  // 電源ON中は常時点灯
  //========================================================

  //=====================ボタン関連==========================
  pinMode(PIN_BUTTON, INPUT_PULLUP); // ボタンピンをプルアップ入力に設定
  #if defined(HAS_ARDUINO_HEADER)
    Serial.begin(9600);              // シリアルモニタ出力を開始
  #endif

  // 起動時点の入力状態を同期
  bool raw = (digitalRead(PIN_BUTTON) == LOW);
  buttonState = raw;
  prevButtonReading = raw;
  //========================================================

  //=================リマインドLED関係=======================
  pinMode(PIN_LED_YELLOW, OUTPUT);
  digitalWrite(PIN_LED_YELLOW, LOW);
  //========================================================

  //====================ブザー関係============================
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  //========================================================

  // 初期状態
  currentState = STATE_IDLE;
  lastRemindMillis = millis();
  lastFeedbackBlinkMillis = 0;
  feedbackLedOn = false;
}

void loop() {

  unsigned long now = millis();


  // ボタン短押しのみでリセット、長押しは無視
  static bool prevBtn = false;
  bool btn = (digitalRead(PIN_BUTTON) == LOW);
  static unsigned long pressStart = 0;
  bool shortPressEvent = false;

  if (btn && !prevBtn) {
    // 押し始め
    pressStart = now;
  }
  if (!btn && prevBtn) {
    // 離した瞬間
    unsigned long pressTime = now - pressStart;
    if (pressTime < longPressThreshold && pressTime > debounceDelay) {
      shortPressEvent = true;
    }
  }
  prevBtn = btn;

  // 短押しのみリセット
  if (shortPressEvent) {
    resetByButton(true);
  }

  // 待機中のときだけリマインド遷移判定
  if (currentState == STATE_IDLE) {
    checkRemindTimer();

    // フィードバック終了判定
    if (isFeedbackActive && (now - feedbackStartMillis >= feedbackDuration)) {
      isFeedbackActive = false;
      feedbackLedOn = false;
      analogWrite(PIN_LED_YELLOW, 0);
    }
  }

  if (currentState == STATE_REMIND || currentState == STATE_WARNING) {
    checkWarningTimeout();
  }

  // LED反映
  updateStatusLeds();
}
