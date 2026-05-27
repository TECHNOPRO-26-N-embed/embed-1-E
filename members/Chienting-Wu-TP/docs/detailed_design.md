# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->
作成者: 呉建廷 / 日付: 2026-05-25 / グループ: 1-E

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 水分補給リマインダー |
| 状態の種類（1-2 状態遷移から） | 0:待機中, 1:リマインド中, 2:警告中 |
| 実装する関数の数（2-2 関数一覧から） | 7個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 43B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
<<<<<<< HEAD
【ピン定義】（basic_design.md 3-1 から転記）
=======
【ピン定義】
>>>>>>> 35da06984db26ea8bc874f5ac0bd3c609bf12c06
  PIN_BUTTON      : const int = 2    // タクトスイッチ（INPUT_PULLUP）
  PIN_LED_GREEN   : const int = 9    // 緑LED（電源ON表示）
  PIN_LED_YELLOW  : const int = 10   // 黄色LED（リマインド/警告/フィードバック表示）
  PIN_BUZZER      : const int = 12   // アクティブブザー

<<<<<<< HEAD
【状態管理】（basic_design.md 1-2 の状態名から転記）
=======
【状態管理】
>>>>>>> 35da06984db26ea8bc874f5ac0bd3c609bf12c06
  currentState  : int = 0   // 0:待機中 1:リマインド中 2:警告中

【状態ID定数】
  STATE_IDLE      : const int = 0
  STATE_REMIND    : const int = 1
  STATE_WARNING   : const int = 2

<<<<<<< HEAD
【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastRemindMillis      : unsigned long = 0   // 最後にリマインド時刻を更新した時刻
  warningStartMillis    : unsigned long = 0   // リマインド開始時刻（30秒経過判定基準）
  lastAlertMillis       : unsigned long = 0   // 最後にブザーを鳴らした時刻
  lastButtonMillis      : unsigned long = 0   // デバウンス確定時刻
  feedbackStartMillis   : unsigned long = 0   // 黄色LEDフィードバック開始時刻
  buttonPressStartMillis: unsigned long = 0   // 長押し開始時刻

【時間定数（ms）】
  remindInterval       : unsigned long = 1800000       // リマインド通知までの待機時間（初期値30分）
  warningDelay         : const unsigned long = 30000   // リマインド開始後、警告状態へ移行するまでの猶予時間
  alertRepeatInterval  : unsigned long = 30000         // 警告中にブザーを再鳴動する間隔
  feedbackDuration     : const unsigned long = 1000    // 待機中ボタン押下時の黄色LEDフィードバック表示時間
  debounceDelay        : const unsigned long = 50      // ボタンのチャタリングを無効化する判定時間
  longPressThreshold   : const unsigned long = 2000    // 長押しとして扱う最小押下時間
=======
【タイマー（millis()用）】
  lastRemindMillis        : unsigned long = 0   // 最後にリマインド時刻を更新した時刻
  warningStartMillis      : unsigned long = 0   // リマインド開始時刻（30秒経過判定基準）
  lastAlertMillis         : unsigned long = 0   // 最後にブザーを鳴らした時刻
  lastButtonMillis        : unsigned long = 0   // デバウンス確定時刻
  feedbackStartMillis     : unsigned long = 0   // 黄色LEDフィードバック開始時刻
  buttonPressStartMillis  : unsigned long = 0   // 長押し開始時刻
  lastFeedbackBlinkMillis : unsigned long = 0   // 黄色LED点滅の反転時刻

【時間定数（ms）】
  remindInterval          : unsigned long = 1800000       // リマインド通知までの待機時間（初期値30分、テスト時は10000）
  warningDelay            : const unsigned long = 30000   // リマインド開始後、警告状態へ移行するまでの猶予時間
  alertRepeatInterval     : unsigned long = 30000         // 警告中にブザーを再鳴動する間隔
  feedbackDuration        : const unsigned long = 1000    // 待機中ボタン押下時の黄色LEDフィードバック表示時間
  debounceDelay           : const unsigned long = 50      // ボタンのチャタリングを無効化する判定時間
  longPressThreshold      : const unsigned long = 2000    // 長押しとして扱う最小押下時間
  feedbackBlinkInterval   : const unsigned long = 250     // 黄色LED点滅の反転間隔
  buzzerOnDuration        : const unsigned long = 120     // ブザー1回の鳴動時間
  buzzerGapDuration       : const unsigned long = 120     // 2回鳴動の間隔
>>>>>>> 35da06984db26ea8bc874f5ac0bd3c609bf12c06

【入力状態】
  buttonState          : bool = false   // 現在の確定状態（押下=true）
  prevButtonReading    : bool = false   // 前回読取値
  isLongPressHandled   : bool = false   // 同一長押しの重複処理防止

【フィードバック・通知状態】
<<<<<<< HEAD
  isFeedbackActive     : bool = false   // 待機中ボタン押下の黄色LED点滅中か
  currentPattern       : int = 0        // 通知パターン番号（0:標準）
  intervalModeIndex    : int = 0        // 0:30分 1:45分 2:60分

【その他のフラグ・カウンター】
  buzzerPulseCount     : int = 0        // 開始時2回鳴動の回数管理
=======
  isFeedbackActive        : bool = false   // 待機中ボタン押下の黄色LED点滅中か
  currentPattern          : int = 0        // 通知パターン番号（0:標準）
  intervalModeIndex       : int = 0        // 0:30分 1:45分 2:60分
  feedbackLedOn           : bool = false   // 黄色LED点滅のON/OFF状態

【その他のフラグ・カウンター】
  buzzerPulseCount        : int = 0        // ブザー鳴動の回数管理
>>>>>>> 35da06984db26ea8bc874f5ac0bd3c609bf12c06
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

【処理の流れ】
```
1. ピンモードを設定する
  - pinMode(PIN_BUTTON, INPUT_PULLUP)
  - pinMode(PIN_LED_GREEN, OUTPUT)
  - pinMode(PIN_LED_YELLOW, OUTPUT)
  - pinMode(PIN_BUZZER, OUTPUT)

  // 各部品の入出力方向を最初に確定し、誤動作を防ぐ
```
```
2. 出力の初期状態を安全側にそろえる
  - digitalWrite(PIN_LED_GREEN, LOW)
  - digitalWrite(PIN_LED_YELLOW, LOW)
  - digitalWrite(PIN_BUZZER, LOW)

  // 起動直後にLEDやブザーが意図せず動かないよう、いったん全OFFにする
```
```
3. 状態管理・タイマー変数を初期化する
  - currentState = STATE_IDLE
  - lastRemindMillis = millis()
  - warningStartMillis = 0
  - lastAlertMillis = 0
  - lastButtonMillis = 0
  - feedbackStartMillis = 0
  - buttonPressStartMillis = 0
  - isFeedbackActive = false
  - isLongPressHandled = false
  - buzzerPulseCount = 0

  // 状態遷移と時間判定の基準値を初期化し、起動時の判定ズレを防ぐ
```
```
4. 運用パラメータを初期値に設定する
  - remindInterval = 1800000（30分）
  - alertRepeatInterval = 30000（30秒）
  - intervalModeIndex = 0
  - currentPattern = 0

  // リマインド間隔・通知間隔・モード番号を既定値にそろえる
```
```
5. 電源ON状態を表示する
  - digitalWrite(PIN_LED_GREEN, HIGH)
  - digitalWrite(PIN_LED_YELLOW, LOW)

  // 緑LED常時点灯で動作中デバイスであることを示し、黄色LEDは待機表示で消灯
```
```
6. デバッグ用シリアルを開始する（任意）
  - Serial.begin(9600)
  
  // 状態や時刻のログ確認を可能にしてデバッグをしやすくする
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

【処理の流れ】
```
＜毎ループ実行すること＞
  - now = millis() を取得する
  - pressed = readButton() で押下イベントを取得する
  - resetByButton(pressed) を呼び、押下時は待機状態へ戻す
  - updateStatusLeds(currentState) を呼び、現在状態に応じたLED表示を更新する

  // 長押しによる任意機能
  - 長押しが確定した場合は changeRemindInterval() を呼び出す
  - 別の長押し条件を満たした場合は changeAlertPattern() を呼び出す
```
```
＜currentState が STATE_IDLE（待機中）のとき＞
  - 緑LEDは点灯維持、黄色LEDは消灯（フィードバック中のみ点滅）
  - checkRemindTimer() を呼び、remindInterval の経過を判定する
  - remindInterval を経過したら
    - currentState = STATE_REMIND
    - warningStartMillis = now
    - lastAlertMillis = 0
    - buzzerPulseCount = 0
  - isFeedbackActive が true かつ now - feedbackStartMillis >= feedbackDuration の場合
    - isFeedbackActive = false にして黄色LED点滅を終了する
```
```
＜currentState が STATE_REMIND（リマインド中）のとき＞
  - 黄色LEDを点灯して水分補給を促す
  - checkWarningTimeout() を呼び、warningDelay（30秒）無反応かを判定する
  - 30秒無反応の場合
    - currentState = STATE_WARNING
    - lastAlertMillis = now
    - buzzerPulseCount = 0
  - ボタン押下時は resetByButton(pressed) 側で待機状態へ戻る
```
```
＜currentState が STATE_WARNING（警告中）のとき＞
  - 黄色LED点灯を維持する
  - 警告開始直後にブザーを2回鳴らす（buzzerPulseCount で回数管理）
  - その後は now - lastAlertMillis >= alertRepeatInterval のたびに再鳴動する
  - ボタン押下時は警告を解除し、待機状態へ戻る（resetByButton）
```

＜補足＞
  - どの状態でもボタン押下で待機状態へ戻れる構造にする
  - すべての時間判定は millis() 差分で行い、delay() は使わない

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `readButton()` — チャタリングを除去して押下イベントを判定する

**basic_design.md 2-2 との対応：** C01（共通）ボタン読出

**引数：** なし

**戻り値：** bool（このループで新規押下が確定したら true）

```
【処理の流れ】
1. raw = digitalRead(PIN_BUTTON) を読む（INPUT_PULLUPのため LOW=押下）
2. pressedNow = (raw == LOW) に変換する
3. 前回読取値 prevButtonReading と異なる場合、lastButtonMillis = now に更新する
4. now - lastButtonMillis >= debounceDelay のときだけ buttonState を確定更新する
5. buttonState が false→true に変化した瞬間だけ true を返す
6. prevButtonReading = pressedNow に更新して終了する

【エラー・異常ケース】
- 入力が不安定な場合: デバウンス時間内は状態を確定しない
- 常時押下状態の場合: エッジ判定により true は最初の1回のみ返す
```

---

### `checkRemindTimer()` — 待機中にリマインド間隔の経過を判定する

**basic_design.md 2-2 との対応：** F01 必須：一定時間ごとにLEDで促す

**引数：** なし

**戻り値：** なし（void）

```
【処理の流れ】
1. currentState が STATE_IDLE でない場合は何もしない
2. now - lastRemindMillis >= remindInterval を判定する
3. 条件成立時、currentState = STATE_REMIND に遷移する
4. warningStartMillis = now、lastAlertMillis = 0、buzzerPulseCount = 0 に初期化する
5. 条件未成立時は待機を継続する

【エラー・異常ケース】
- remindInterval が 0 の場合: 誤動作防止のため最小値（例: 60000ms）を適用して判定する
```

---

### `updateStatusLeds()` — 状態に応じて緑/黄色LED表示を更新する

**basic_design.md 2-2 との対応：** F02 必須：LEDでリマインド状態を表示

**引数：** なし（グローバル変数 currentState を参照）

**戻り値：** なし（void）

```
【処理の流れ】
1. 緑LEDは電源ON中のため常時 HIGH にする
2. currentState が STATE_IDLE の場合
  - isFeedbackActive が true なら黄色LEDを点滅させる（feedbackLedOn, lastFeedbackBlinkMillis, feedbackBlinkInterval を利用）
  - それ以外は黄色LEDを LOW にする
3. currentState が STATE_REMIND または STATE_WARNING の場合、黄色LEDを HIGH にする
4. それ以外の未定義状態は安全側として黄色LEDを LOW にする

【エラー・異常ケース】
- 状態が未定義値の場合: 待機表示（黄色LED OFF）にフォールバックする
```

---

### `checkWarningTimeout()` — リマインド中30秒無反応で警告遷移し、2回鳴動・再鳴動を管理する

**basic_design.md 2-2 との対応：** F03 必須：30秒無反応でブザー警告

**引数：** なし

**戻り値：** なし（void）

```
【処理の流れ】
1. currentState が STATE_REMIND の場合
  - now - warningStartMillis >= warningDelay を判定
  - 成立時は currentState = STATE_WARNING に遷移
  - 警告シーケンス用 static変数（sequenceActive, buzzerOn, phaseStartMillis）を初期化
  - buzzerPulseCount = 0

2. currentState が STATE_WARNING の場合
  - buzzerPulseCount < 2 の間は
    - buzzerOn=false なら、前回OFFからbuzzerGapDuration経過でON
    - buzzerOn=true なら、buzzerOnDuration経過でOFF＋buzzerPulseCount++
    - 2回鳴動後はsequenceActive=false, lastAlertMillis=now
  - buzzerPulseCount >= 2 かつ alertRepeatInterval経過で
    - sequenceActive=true, buzzerPulseCount=0 で2回鳴動シーケンス再開

3. それ以外の状態では
  - sequenceActive, buzzerOnをfalse、ブザーOFF

【エラー・異常ケース】
- ブザー出力が異常な場合: ピン出力を LOW に戻し、次周期で再判定
- warningStartMillis 未初期化の場合: now を代入して判定基準を再同期
```

---

### `resetByButton(bool pressed)` — 押下時に警告解除・タイマー初期化・待機復帰を行う

**basic_design.md 2-2 との対応：** F04 必須：ボタンでリセット

**引数：** pressed（bool）: readButton() で確定した押下イベント

**戻り値：** なし（void）

```
【処理の流れ】
1. pressed が false の場合は何もしない
2. pressed が true の場合、currentState = STATE_IDLE に設定する
3. lastRemindMillis = now に更新し、次回リマインド起点をリセットする
4. warningStartMillis / lastAlertMillis / buzzerPulseCount を初期化する
5. ブザーを LOW にして鳴動を停止する
6. isFeedbackActive = true、feedbackStartMillis = now にして黄色LED点滅を開始する

【エラー・異常ケース】
- 警告中に押下された場合: 最優先でブザー停止と待機復帰を実行する
- 連打された場合: readButton() 側のデバウンスで多重実行を抑制する
```

---

### `changeRemindInterval(bool longPress)` — 長押しでリマインド間隔を切り替える

**basic_design.md 2-2 との対応：** A01 追加：リマインド間隔変更

**引数：** longPress（bool）: 長押し確定イベント

**戻り値：** なし（void）

```
【処理の流れ】
1. longPress が false の場合は何もしない
2. intervalModeIndex を 0→1→2→0 の順で更新する
3. intervalModeIndex に応じて remindInterval を設定する
  - 0: 1800000（30分）
  - 1: 2700000（45分）
  - 2: 3600000（60分）
4. 切替直後に lastRemindMillis = now として次回判定を再スタートする

【エラー・異常ケース】
- intervalModeIndex が範囲外の場合: 0 に補正して 30分モードへ戻す
```

---

### `changeAlertPattern(bool longPress)` — 長押しでLED/ブザーパターンを切り替える

**basic_design.md 2-2 との対応：** A02 追加：LED/ブザーパターン切替

**引数：** longPress（bool）: 長押し確定イベント

**戻り値：** なし（void）

```
【処理の流れ】
1. longPress が false の場合は何もしない
2. currentPattern を 0→1→2→0 の順で更新する
3. currentPattern に応じて以下の出力パラメータを切り替える
  - 警告開始時の鳴動回数
  - 再鳴動間隔（alertRepeatInterval）
  - 黄色LEDの点滅パターン
4. 切替内容を次回の checkWarningTimeout() / updateStatusLeds() に反映する

【エラー・異常ケース】
- currentPattern が範囲外の場合: 0（標準）へ戻す
```

---

## 3. 重要ロジックの詳細設計


### 3-1. チャタリング防止（デバウンス処理・LED点滅・2回鳴動シーケンス管理の補足）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。
  INPUT_PULLUP を使うため、LOW を押下として扱う。

【処理の流れ】
  1. raw = digitalRead(PIN_BUTTON) を読む
  2. pressedNow = (raw == LOW) に変換する（LOW=押下）
  3. pressedNow が prevButtonReading と異なる場合、lastButtonMillis = now に更新する
  4. now - lastButtonMillis < debounceDelay の間は、確定状態 buttonState を更新しない
  5. now - lastButtonMillis >= debounceDelay なら、buttonState = pressedNow として確定する
  6. buttonState が false→true に変化した瞬間を押下イベントとして採用する
  7. prevButtonReading = pressedNow に更新して次ループへ進む

【必要な変数（Section 1 に追加済みか確認）】
  lastButtonMillis : unsigned long         // 入力変化を検出した時刻
  debounceDelay    : const unsigned long   // チャタリング判定時間（50ms）
  prevButtonReading: bool                  // 前回の生入力状態
  buttonState      : bool                  // デバウンス後の確定状態
  lastFeedbackBlinkMillis : unsigned long  // 黄色LED点滅の反転時刻
  feedbackBlinkInterval   : const unsigned long // 黄色LED点滅の反転間隔
  feedbackLedOn           : bool           // 黄色LED点滅のON/OFF状態
  buzzerPulseCount        : int            // ブザー鳴動の回数管理
  buzzerOnDuration        : const unsigned long // ブザー1回の鳴動時間
  buzzerGapDuration       : const unsigned long // 2回鳴動の間隔
  （checkWarningTimeout()内static変数: sequenceActive, buzzerOn, phaseStartMillis）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。
  delay() を使わず、複数のタイミング判定を並行して管理できる。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  - リマインド間隔判定（lastRemindMillis, remindInterval）
    → 待機中に「now - lastRemindMillis >= remindInterval」でリマインド状態へ遷移
  - 警告遷移判定（warningStartMillis, warningDelay）
    → リマインド中に「now - warningStartMillis >= warningDelay」で警告状態へ遷移
  - 警告中の再鳴動（lastAlertMillis, alertRepeatInterval）
    → 警告中に「now - lastAlertMillis >= alertRepeatInterval」でブザー再鳴動
  - ボタンデバウンス（lastButtonMillis, debounceDelay）
    → 入力変化から「now - lastButtonMillis >= debounceDelay」で確定状態に反映
  - フィードバックLED点滅（feedbackStartMillis, feedbackDuration）
    → ボタン押下時「now - feedbackStartMillis < feedbackDuration」だけ黄色LED点滅
  - 長押し判定（buttonPressStartMillis, longPressThreshold）
    → ボタン押下継続中「now - buttonPressStartMillis >= longPressThreshold」で長押し確定
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1.
2.
3.

【入力値と出力値の関係】

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |
| 4 |  |  |  |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | タクトスイッチを1回押す | true が返る | | [ ] |
| 2 | readButton() | スイッチを素早く2回押す | 1回分だけ true になる | | [ ] |
| 3 | readButton() | スイッチを長押しする | 長押し判定が1回だけ発生 | | [ ] |
| 4 | readButton() | スイッチを押しっぱなし | 2回目以降は true にならない | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateStatusLeds(STATE_IDLE) | state=0（待機中）を渡す | 緑LEDが点灯、黄色LEDは消灯 | | [ ] |
| 2 | updateStatusLeds(STATE_REMIND) | state=1（リマインド中）を渡す | 緑LEDが点灯、黄色LEDが点灯 | | [ ] |
| 3 | updateStatusLeds(STATE_WARNING) | state=2（警告中）を渡す | 緑LEDが点灯、黄色LEDが点灯 | | [ ] |
| 4 | updateStatusLeds(STATE_IDLE, isFeedbackActive=true) | フィードバック中 | 黄色LEDが点滅 | | [ ] |
| 5 | checkWarningTimeout() | 警告遷移直後 | ブザーが2回鳴る | | [ ] |
| 6 | checkWarningTimeout() | 警告中、alertRepeatInterval経過 | ブザーが再鳴動する | | [ ] |
| 7 | changeAlertPattern() | パターン切替時 | LED/ブザーのパターンが切り替わる | | [ ] |

### 5-3. タイミング・並行動作テスト

> ※ delay()を使わず、millis()による非ブロッキング制御や複数タイマーの並行動作が正しく機能するかを検証します。
> 状態遷移・LED点滅・ブザー再鳴動・デバウンス・長押し判定など、全てのタイミング依存処理を網羅してください。

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中にボタンを押す | ボタン入力が無視されない | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅やリマインド間隔をストップウォッチで確認 | 設計した周期通りに動作 | | [ ] |
| 3 | 複数タイマーの並行動作 | LED点滅・リマインド・警告・デバウンスを同時に発生させる | すべて正しく動作し、相互に影響しない | | [ ] |
| 4 | デバウンス判定のタイミング精度 | 50ms未満の連打を試す | 1回分しか反応しない | | [ ] |
| 5 | 長押し判定のタイミング精度 | 2秒未満/2秒以上で押し分ける | 2秒以上のみ長押し判定 | | [ ] |
| 6 | フィードバックLEDの点滅時間 | ボタン押下時の点滅を計測 | 設計通りの時間だけ点滅 | | [ ] |
| 7 | （自分のタイミング依存処理を追加） | | | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- 設計全体は要件・基本設計と整合しており、致命的な曖昧さやズレはありません。
- millis()タイマー管理・デバウンス・長押し判定・状態遷移の安全側処理も明記されており、型も適切です。
- センサー処理（readSensor）は本設計では未使用であり、関連記述は削除済みです。
- 変数初期化や異常値ガードも各関数で明記されています。

**対応した内容：**
- センサー未使用部分の記述を整理し、実装仕様と設計書のズレを解消。
- 状態遷移・タイマー・デバウンス等の安全側処理・初期化を全体で徹底。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系（readButton）、出力系（LED/ブザー）、タイミング系（millis, デバウンス, 長押し等）すべて網羅されています。
- 境界値（50ms未満/以上の連打、2秒未満/以上の長押し等）や異常系（未定義状態、異常値入力）もテスト項目に含まれています。
- センサー関連テストは削除済みで、現仕様に合致しています。

**対応した内容：**
- テスト項目の不要部分（センサー）を削除し、現仕様に即した網羅的なテスト表に整理。
- 境界値・異常系・パターン切替など、実装上のリスクが高い部分もテスト項目に含めています。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 詳細設計から関数増えた? | 神谷 | 変わってない |
| 2 | lastRemindMillisどのようなものですか? | 神谷 | 待機中からリマインドまでの時間換算用です |


### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
