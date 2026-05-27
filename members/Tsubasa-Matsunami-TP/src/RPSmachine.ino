// ========================================
// 電子じゃんけんバトルマシン
// Arduino UNO R3
// ========================================

// ---------- ピン定義 ----------
const int PIN_BUTTON_GU = 2;
const int PIN_BUTTON_CHOKI = 3;
const int PIN_BUTTON_PA = 4;

const int PIN_LED = 13;
const int PIN_BUZZER = 8;

// ---------- 状態管理 ----------
int currentState = 0;
// 0:入力待機
// 1:勝敗判定
// 2:結果表示

// ---------- プレイヤー・CPUの手 ----------
int playerHand = -1;
// 0:グー
// 1:チョキ
// 2:パー

int cpuHand = -1;

// ---------- 勝敗結果 ----------
int gameResult = -1;
// 0:あいこ
// 1:勝ち
// 2:負け

// ---------- タイマー ----------
unsigned long lastButtonTime = 0;
unsigned long resultStartTime = 0;

const int DEBOUNCE_DELAY = 50;
const int RESULT_DISPLAY_TIME = 2000;

// ========================================
// setup()
// ========================================
void setup() {

  pinMode(PIN_BUTTON_GU, INPUT_PULLUP);
  pinMode(PIN_BUTTON_CHOKI, INPUT_PULLUP);
  pinMode(PIN_BUTTON_PA, INPUT_PULLUP);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  Serial.begin(9600);

  digitalWrite(PIN_LED, HIGH);
  tone(PIN_BUZZER, 1000);
  delay(300);

  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  randomSeed(analogRead(A0));
}

// ========================================
// loop()
// ========================================
void loop() {

  switch (currentState) {
    case 0:

      playerHand = inputHand();

      if (playerHand != -1) {
        currentState = 1;
      }

      break;

    case 1:

      cpuHand = cpuSelectHand();

      gameResult = judgeResult(playerHand, cpuHand);

      resultStartTime = millis();

      currentState = 2;

      break;

    case 2:

      displayResult(gameResult);

      if (millis() - resultStartTime >= RESULT_DISPLAY_TIME) {

        noTone(PIN_BUZZER);
        digitalWrite(PIN_LED, LOW);

        playerHand = -1;
        cpuHand = -1;
        gameResult = -1;

        currentState = 0;
      }

      break;
  }
}

// ========================================
// プレイヤー入力
// ========================================
int inputHand() {

  unsigned long now = millis();

  if (now - lastButtonTime < DEBOUNCE_DELAY) {
     return -1;
   }

  bool guPressed =
    digitalRead(PIN_BUTTON_GU) == LOW;

   bool chokiPressed =
     digitalRead(PIN_BUTTON_CHOKI) == LOW;

   bool paPressed =
    digitalRead(PIN_BUTTON_PA) == LOW;

  int pressCount =
    guPressed + chokiPressed + paPressed;

  if (pressCount > 1) {
    return -1;
  }

  if (guPressed) {
    lastButtonTime = now;
    return 0;
  }

   if (chokiPressed) {
    lastButtonTime = now;
    return 1;
  }

  if (paPressed) {
    lastButtonTime = now;
    return 2;
  }

  return -1;
}

// ========================================
// CPUの手を決定
// ========================================
int cpuSelectHand() {

  return random(0, 3);
}

// ========================================
// 勝敗判定
// ========================================
int judgeResult(
  int player,
  int cpu
) {

  if (player == cpu) {
    return 0;
  }

  if (
    (player == 0 && cpu == 1) ||
    (player == 1 && cpu == 2) ||
    (player == 2 && cpu == 0)
  ) {
    return 1;
  }

  return 2;
}

// ========================================
// 結果表示
// ========================================
void displayResult(int result) {

  switch (result) {

    case 0:
    digitalWrite(PIN_LED, HIGH);

    tone(PIN_BUZZER, 800);

    break;


    case 1:

    digitalWrite(PIN_LED, HIGH);

    tone(PIN_BUZZER, 1200);

    break;

    case 2:

    digitalWrite(PIN_LED, HIGH);

    tone(PIN_BUZZER, 400);

    break;
  }
}