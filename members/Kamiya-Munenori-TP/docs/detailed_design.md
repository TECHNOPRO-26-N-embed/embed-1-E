# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

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
| 作品タイトル | 休憩延長防止アラーム |
| 状態の種類（1-2 状態遷移から） | 待機中、休憩中、通知中 |
| 実装する関数の数（2-2 関数一覧から） | 8 個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 59　B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_BUTTON    : const uint8_t = 2    // タクトスイッチ（INPUT_PULLUP）
  PIN_BUZZER    : const uint8_t = 12   // アクティブブザー

【状態管理】（basic_design.md 1-2 の状態名から転記）
  state         : uint8_t = 0          // 0:待機中 1:休憩中 2:通知中

【休憩時間・時刻記録】（basic_design.md 2-1 から転記）
  duration      : uint16_t = 600       // 休憩時間（秒）
  startTime     : uint32_t = 0         // 休憩開始時刻（Unix time）
  endTime       : uint32_t = 0         // 休憩終了時刻（Unix time）

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  breakStartMs  : unsigned long = 0    // 休憩開始時のmillis
  btnLastMs     : unsigned long = 0    // ボタン状態が変化した時刻
  debounceMs    : const unsigned long = 50

【入力・出力状態】（basic_design.md 2-1 から転記）
  btnStable     : bool = false         // デバウンス後の確定状態
  btnEvent      : bool = false         // 押下イベントフラグ
  buzzer        : bool = false         // ブザーON/OFF状態

【ログ出力バッファ】
  logBuf        : char[32] = ""       // "YYYY/MM/DD HH:MM:SS"

【その他のフラグ・カウンター】
  rtcError      : bool = false         // RTC通信異常フラグ
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ピンモードを設定する
  - PIN_BUTTON → INPUT_PULLUP
  - ボタン未押下は HIGH、押下は LOW として扱う
  - PIN_BUZZER → OUTPUT（初期はLOW）

2. RTCとシリアルを初期化する
  - Wire.begin() を実行
  - RTC.begin() を実行し、失敗時は rtcError = true にする
  - Serial.begin(9600) を実行

3. 変数の初期状態を設定する
  - state = 0（待機中）
  - btnStable / btnEvent / buzzer を false にする
  - startTime / endTime / breakStartMs / btnLastMs を 0 にする

4. 起動時の確認ログを出力する（任意）
  - rtcError が false の場合: "System ready" を出力
  - rtcError が true の場合: "RTC init error" を出力
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
  - btnEvent = readButton() を実行する
  - now（ローカル変数）= millis() を取得する

＜state が 0（待機中）のとき＞
  - btnEvent が true の場合: startBreak() を呼び、state = 1（休憩中）に遷移する
  - それ以外は待機を継続する

＜state が 1（休憩中）のとき＞
  - 休憩時間の経過を確認する
  - duration に達した場合: setBuzzer(true) で通知開始し、state = 2（通知中）に遷移する
  - btnEvent が true の場合: endBreak() を呼び、state = 0（待機中）に遷移する（早期終了）

＜state が 2（通知中）のとき＞
  - ブザー鳴動状態を維持する
  - btnEvent が true の場合: setBuzzer(false) と endBreak() を実行し、state = 0（待機中）に遷移する

```

---

### `readButton()` — デバウンス後のボタン押下イベントを返す

**basic_design.md 2-2 との対応：** デバウンス後のボタン押下イベントを返す

**引数：** なし

**戻り値：** `bool`（押下イベントがあれば true）

```
【処理の流れ】
1. PIN_BUTTON を読み、HIGH=未押下、LOW=押下として判定する（INPUT_PULLUP前提）。
2. 前回変化時刻 btnLastMs から debounceMs 以上経過しているか確認する。
3. 経過していれば btnStable を更新し、HIGH→LOW（押下エッジ）のときのみ btnEvent=true を返す。

【エラー・異常ケース】
- ボタン値が不安定な場合: チャタリングとして無視し false を返す。
```

---

### `getTime()` — RTCから現在時刻（Unix time）を取得する

**basic_design.md 2-2 との対応：** RTCから現在時刻（Unix time）を取得

**引数：** なし

**戻り値：** `uint32_t`（現在時刻。失敗時は0）

```
【処理の流れ】
1. RTCモジュールから現在時刻を読み出す。
2. 読み出し成功時は rtcError=false にして Unix time を返す。
3. 読み出し失敗時は rtcError=true にして 0 を返す。

【エラー・異常ケース】
- RTC通信失敗時: エラーフラグを立て、呼び出し元で警告出力する。
```

---

### `setBuzzer(bool on)` — ブザーのON/OFFを制御する

**basic_design.md 2-2 との対応：** ブザーON/OFFを制御

**引数：** `on`（`bool`）: trueでON、falseでOFF

**戻り値：** `void`

```
【処理の流れ】
1. 引数 on の値に応じて PIN_BUZZER を HIGH/LOW 出力する。
2. buzzer フラグを on と同じ値に更新する。
3. 必要に応じて状態遷移側で通知開始/停止を判断する。

【エラー・異常ケース】
- 鳴動しない場合: 配線断線の可能性としてシリアル警告対象にする。
```

---

### `startBreak()` — 休憩開始時刻を記録し休憩中へ遷移する

**basic_design.md 2-2 との対応：** 休憩開始時刻を記録し休憩中へ遷移

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. startTime = getTime() で開始時刻を取得する。
2. breakStartMs = millis() を保存し、休憩タイマーの基準時刻にする。
3. printLog("BREAK_START") を実行し、state=1（休憩中）へ遷移する。

【エラー・異常ケース】
- startTime が 0 の場合: rtcError として警告ログを出し、必要なら待機中に戻す。
```

---

### `endBreak()` — 終了時刻を記録し待機中へ遷移する

**basic_design.md 2-2 との対応：** 終了時刻を記録し待機中へ遷移

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. endTime = getTime() で終了時刻を取得する。
2. setBuzzer(false) を呼び、通知中なら鳴動を停止する。
3. printLog("BREAK_END") を実行し、state=0（待機中）へ遷移する。

【エラー・異常ケース】
- endTime が 0 の場合: rtcError として警告ログを出力して終了処理は継続する。
```

---

### `printLog(const char* msg)` — シリアルモニタに時刻ログを出力する

**basic_design.md 2-2 との対応：** シリアルモニタに時刻を出力

**引数：** `msg`（`const char*`）: 出力メッセージ種別

**戻り値：** `void`

```
【処理の流れ】
1. startTime または endTime を "YYYY/MM/DD HH:MM:SS" 形式に整形して logBuf に格納する。
2. msg と logBuf を組み合わせて Serial に出力する。
3. 出力後、必要ならデバッグ用に state も併記する。

【エラー・異常ケース】
- シリアル出力失敗時: 再送を試み、失敗が続く場合は警告メッセージに切り替える。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  INPUT_PULLUP設定のため、ボタン未押下=HIGH、押下=LOW。
  ボタンが押されたとき（HIGH→LOWエッジ）、50ms以内の連続入力は同じ1回の押下として無視する。

【処理の流れ】
  1. digitalRead(PIN_BUTTON)で現在のボタン値（HIGH/LOW）を取得。
  2. 現在値とbtnStable（前回確定値）が異なる場合、btnLastMsにmillis()を記録。
  3. millis() - btnLastMs >= debounceMs なら、btnStableを現在値に更新。
  4. btnStableがHIGH→LOWに変化したときのみ押下イベント（btnEvent=true）とする。
  5. それ以外はbtnEvent=false。

【必要な変数（Section 1 に追加済みか確認）】
  btnLastMs : unsigned long   // ボタン状態が変化した時刻
  debounceMs : const unsigned long = 50  // チャタリング判定時間（ms）
  btnStable : bool            // デバウンス後の確定ボタン状態
  btnEvent : bool            // 押下イベントフラグ
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  休憩時間の計測やボタンのデバウンスなど、一定時間の経過判定にmillis()を利用する。
  休憩タイマーでは「休憩開始時のmillis（breakStartMs）」を記録し、「現在のmillis() - breakStartMs >= duration*1000」で休憩終了を判定する。

【処理の流れ（例: 休憩タイマー）】
  1. 休憩開始時に breakStartMs = millis() を記録
  2. 毎ループで now = millis() を取得
  3. now - breakStartMs >= duration*1000 なら休憩終了（通知開始）
  4. それ以外は休憩継続

【自分のシステムで millis() を使う処理】
  - 休憩時間の経過判定（breakStartMs, duration, now）
  - ボタンのデバウンス判定（btnLastMs, debounceMs, now）
  - 必要に応じて他のタイミング管理にも応用可能
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
| 1 | センサー値が正しく取れているか | `readSensor()` | `Serial.println(sensorValue);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButton()` | `Serial.println("btn confirmed");` |
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
| 3 | readSensor() | センサーを正常範囲で使う | 仕様範囲内の値が返る | | [ ] |
| 4 | readSensor() | センサーを遮蔽・範囲外に向ける | 誤動作しない | | [ ] |
| 5 | （自分の関数を追加） | | | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutput(0) | state=0（待機中）を渡す | 緑LEDが点滅する | | [ ] |
| 2 | updateOutput(1) | state=1（動作中）を渡す | 赤LEDが点灯、ブザーが鳴る | | [ ] |
| 3 | （自分の状態・関数を追加） | | | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中にボタンを押す | ボタン入力が無視されない | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅をストップウォッチで確認 | 設計した周期（例:500ms）通りに点滅 | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

**対応した内容：**

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

**対応した内容：**

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
