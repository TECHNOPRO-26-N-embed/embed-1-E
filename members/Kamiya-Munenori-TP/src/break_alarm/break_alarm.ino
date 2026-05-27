// DS3231 RTCモジュールを使用するためのライブラリをインクルード
#include <Wire.h>
#include <DS3231.h>

// ピン定義
const uint8_t PIN_BUTTON = 2;   // ボタン入力ピン
const uint8_t PIN_BUZZER = 12;  // ブザー出力ピン

// 時間と状態管理用変数
uint8_t state;           // 現在の遷移状態
uint16_t duration = 9 * 60;           // 休憩時間（秒）
RTCDateTime startTime;                // 休憩開始時刻
RTCDateTime endTime;                  // 休憩終了時刻
unsigned long breakStartMs = 0;       // 休憩開始時のミリ秒
unsigned long btnLastMs = 0;          // ボタン状態が変化した時刻
const unsigned long debounceMs = 50;  // デバウンス時間（ms）
unsigned long buzzerMs = 0;           // ブザー開始時のミリ秒

// 入出力状態管理
bool buzzer = false;  // ブザーON/OFF状態

// エラー管理とログ出力用
bool rtcError = false;  // RTCエラーフラグ

// RTCモジュールのインスタンス
DS3231 clock;    // DS3231 RTCモジュールのインスタンス
RTCDateTime dt;  // RTCから取得する日時データ

// --- 関数プロトタイプ宣言 ---
bool readButton();               // ボタン状態を読み取る関数
void setBuzzer(bool on);         // ブザーをON/OFF制御する関数
void startBreak();               // 休憩を開始する関数
void endBreak();                 // 休憩を終了する関数
void printLog(const char *msg);  // ログを出力する関数

void setup() {
  // ピンモードの設定
  pinMode(PIN_BUTTON, INPUT_PULLUP);  // ボタンピンをプルアップ入力に設定
  pinMode(PIN_BUZZER, OUTPUT);        // ブザーピンを出力に設定
  digitalWrite(PIN_BUZZER, LOW);      // ブザーを初期状態でOFFに設定

  // I2C通信とシリアル通信の初期化
  Wire.begin();
  Serial.begin(9600);

  // RTCモジュールの初期化
  clock.begin();
  rtcError = false;  // RTCエラーを初期化
  // clock.setDateTime(__DATE__, __TIME__);  // 必要時のみRTC時刻を設定

  // 初期状態の設定
  state = 0;  // 初期状態を待機中:0に設定
  buzzer = false;
  breakStartMs = 0;
  btnLastMs = 0;
  buzzerMs = 0;

  // 初期化結果をシリアルモニタに出力
  if (!rtcError) {
    Serial.println("System ready");
  } else {
    Serial.println("RTC init error");
  }
}

void loop() {
  // ボタン状態を読み取り、イベントを検出
  bool btnEvent = readButton();
  unsigned long nowMs = millis();  // 現在時刻（ミリ秒）を取得

  // 休憩中の1秒ブザー停止処理
  if (state == STATE_BREAK && buzzer && buzzerMs != 0 && (nowMs - buzzerMs >= 1000UL)) {
    setBuzzer(false);
  }

  // 現在の状態に応じた処理を実行
  switch (state) {
    case 0:  // 待機中
      if (btnEvent) {
        startBreak();  // ボタンが押されたら休憩開始処理を呼び出す
      }
      break;

    case 1:  // 休憩中
      if ((nowMs - breakStartMs) >= (unsigned long)duration * 1000UL) {
        setBuzzer(true);  // 休憩時間が経過したらブザーをONにする
        state = 2;  // 通知中に遷移
      } else if (btnEvent) {
        endBreak();  // 休憩中にボタンが押されたら休憩終了処理を呼び出す
      }
      break;

    case 2:  // 通知中
      if (btnEvent) {
        setBuzzer(false);  // 通知中にボタンが押されたらブザーをOFFにする
        endBreak();  // 休憩終了処理を呼び出す
      }
      break;

    case 3:  // エラー状態
      // エラー状態の処理
      printLog("RTC_ERROR\n");
      state = 0;  // 待機中に遷移
      break;

    default:
      state = 0;  // 待機中に遷移
      break;
  }
}

bool readButton() {
  bool rawPressed = !digitalRead(PIN_BUTTON);  // true when pressed (LOW)
  static bool btnStable = false;
  static bool lastRawPressed = false;

  if (rawPressed != lastRawPressed) {  // ボタン状態が変化した場合
    btnLastMs = millis();
    lastRawPressed = rawPressed;
  }

  if ((millis() - btnLastMs) >= debounceMs && rawPressed != btnStable) {  // デバウンス時間が経過し、状態が安定している場合
    btnStable = rawPressed;
    if (btnStable) {
      return true;
    }
  }

  return false;
}

void setBuzzer(bool on) {  // ブザーのON/OFFを制御する関数
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  buzzer = on;

  if (on) {
    buzzerMs = millis();  // ブザー開始時刻を記録
  } else {
    buzzerMs = 0;  // ブザー停止時刻をリセット
  }
}

void startBreak() {                 // 休憩開始処理
  startTime = clock.getDateTime();  // RTCから現在時刻を取得
  if (startTime.year < 2000) {      // RTC未設定などの簡易判定
    state = 3;  // エラー状態に遷移
    return;
  }
  setBuzzer(true);  // 休憩開始時にブザーをONにする
  breakStartMs = millis();
  printLog("BREAK_START");
  state = 1;  // 休憩中に遷移
}

void endBreak() {                 // 休憩終了処理
  endTime = clock.getDateTime();  // RTCから現在時刻を取得
  setBuzzer(false);               // 休憩終了時にブザーをOFFにする
  if (endTime.year < 2000) {      // RTC未設定などの簡易判定
    state = 3;  // エラー状態に遷移
    return;
  }
  printLog("BREAK_END");
  state = 0;  // 待機中に遷移
}

void printLog(const char *msg) {
  // RTCDateTimeの生データをそのまま出力
  if (strcmp(msg, "BREAK_START") == 0) {
    Serial.print("休憩開始 : ");
    Serial.print(startTime.year);
    Serial.print("/");
    Serial.print(startTime.month);
    Serial.print("/");
    Serial.print(startTime.day);
    Serial.print(" ");
    Serial.print(startTime.hour);
    Serial.print(":");
    Serial.print(startTime.minute);
    Serial.print(":");
    Serial.print(startTime.second);
    Serial.print("\t");
  } else if (strcmp(msg, "BREAK_END") == 0) {
    Serial.print("休憩終了 : ");
    Serial.print(endTime.year);
    Serial.print("/");
    Serial.print(endTime.month);
    Serial.print("/");
    Serial.print(endTime.day);
    Serial.print(" ");
    Serial.print(endTime.hour);
    Serial.print(":");
    Serial.print(endTime.minute);
    Serial.print(":");
    Serial.print(endTime.second);
    if (state == 1) {  // 休憩中
      Serial.println(" (早期終了)\n");
    } else {
      Serial.println("\n");
    }
  } else {
    Serial.print(msg);
  }
}
