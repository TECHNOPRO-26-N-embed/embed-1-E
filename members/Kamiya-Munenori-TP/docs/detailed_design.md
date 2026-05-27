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
| 状態の種類（1-2 状態遷移から） | 待機中、休憩中、通知中、エラー |
| 実装する関数の数（2-2 関数一覧から） | 7 個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 69　B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_BUTTON    : const uint8_t = 2    // タクトスイッチ（INPUT_PULLUP）
  PIN_BUZZER    : const uint8_t = 12   // アクティブブザー

【状態管理】（basic_design.md 1-2 の状態名から転記）
  state         : uint8_t = 0          // 0:待機中 1:休憩中 2:通知中 3:エラー

【休憩時間・時刻記録】
  duration      : uint16_t = 9*60      // 休憩時間（秒）
  startTime     : RTCDateTime          // 休憩開始時刻（RTCDateTime型）
  endTime       : RTCDateTime          // 休憩終了時刻（RTCDateTime型）

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  breakStartMs  : unsigned long = 0    // 休憩開始時のmillis
  btnLastMs     : unsigned long = 0    // ボタン状態が変化した時刻
  debounceMs    : const unsigned long = 50

【ブザー制御用タイマー】
  buzzerMs      : unsigned long = 0    // ブザー開始時のmillis（1秒非同期制御用）

【入力・出力状態】
  buzzer        : bool = false         // ブザーON/OFF状態



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
  - PIN_BUTTON → INPUT_PULLUP
  - ボタン未押下は HIGH、押下は LOW として扱う
  - PIN_BUZZER → OUTPUT（初期はLOW） //OFF

2. RTCとシリアルを初期化する
  - Wire.begin() を実行 // I2C通信開始（RTCなどの外部デバイスとやりとりするため）
  - Serial.begin(9600) を実行 // シリアル通信開始（PCへのログ出力用）
  - Wire.beginTransmission(0x68) と Wire.endTransmission() でRTCのI2C応答を確認する
  - rtcStatus != 0 の場合は rtcError = true とする // ACKなしでRTC未接続を検出
  - rtcError == false の場合のみ clock.begin() を実行する // RTC（時計モジュール）の初期化

3. 変数の初期状態を設定する
  - state = 0 //待機中
  - buzzer を false にする // ブザー状態をリセット
  - breakStartMs / btnLastMs / buzzerMs を 0 にする // タイマー関連を初期化

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
  - btnEvent = readButton() を実行する  // ボタンが押されたか判定
  - nowMs（ローカル変数）= millis() を取得する // 現在時刻（ms）を取得
  - 休憩中（state==1）でブザーが鳴動中かつ1秒経過した場合、setBuzzer(false) で自動停止する //休憩開始時に1秒ブザーを鳴動させる

＜state が 0（待機中）のとき＞
  - btnEvent が true の場合: startBreak() を呼び、state = 1（休憩中）に遷移する // ボタン押下で休憩開始(関数を用いて)
  - それ以外は待機を継続する // 何もしない

＜state が 1（休憩中）のとき＞
  - 休憩時間の経過を確認する // タイマーで経過判定
  - duration に達した場合: setBuzzer(true) で通知開始し、state = 2（通知中）に遷移する // 時間経過で通知
  - btnEvent が true の場合: endBreak() を呼び、state = 0（待機中）に遷移する（早期終了） // ボタン押下で早期終了

＜state が 2（通知中）のとき＞
  - btnEvent が true の場合:endBreak() を実行し、state = 0（待機中）に遷移する // 停止条件：ボタン押下で通知終了

＜state が 3（エラー）のとき＞
  - printLog("RTC_ERROR\n") を実行し、state = 0（待機中）に遷移する // エラー内容をシリアルモニタに出力し、待機中に戻す
```

---

### `readButton()` — デバウンス後のボタン押下イベントを返す

**basic_design.md 2-2 との対応：** ボタンが押された瞬間だけ true を返す。チャタリング防止のため50msのデバウンス処理を行う。

**引数：** なし

**戻り値：** `bool`（押下イベントがあれば true）

```
【処理の流れ】
1. rawPressed = !digitalRead(PIN_BUTTON) で現在の押下状態（押下=true）を取得する。 // INPUT_PULLUPなのでLOWを押下として扱う
2. lastRawPressed（static）と値が変わったら、btnLastMs = millis() を記録し、lastRawPressed を更新する。 // 生入力の変化時刻を記録
3. millis() - btnLastMs >= debounceMs (前回入力から十分時間が経つ)かつ rawPressed != btnStable(状態が本当に変わった) のとき、btnStable（static）を更新する。 // 一定時間同じ状態なら確定
4. btnStable が true（押下確定）になった瞬間だけ true を返す。 // 押下イベントを1回だけ返す
5. それ以外は false を返す。 // 未押下またはチャタリング中はイベントなし

【エラー・異常ケース】
- ボタン値が不安定な場合: チャタリングとして無視し false を返す。
```

---

### `setBuzzer(bool on)` — ブザーのON/OFFを制御する

**basic_design.md 2-2 との対応：** ブザーON/OFFを制御

**引数：** `on`（`bool`）: trueでON、falseでOFF

**戻り値：** `void`

```
【処理の流れ】

1. onがtrueならブザーON、falseならOFFにする // ブザー制御
2. buzzerフラグも同じ値にする // 状態記録
3. on==trueでブザーを鳴らし始めた場合、buzzerMs = millis() を記録する // 1秒非同期制御用
4. on==falseでOFFにした場合、buzzerMs = 0 にリセットする
5. （通知の開始・停止は呼び出し元で判断）

```

---

### `startBreak()` — 休憩開始時刻を記録し休憩中へ遷移する

**basic_design.md 2-2 との対応：** 休憩開始時刻を記録し休憩中へ遷移

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. startTime = clock.getDateTime() でRTCから直接開始時刻を取得する。
2. startTime.year < 2000 の場合は state=3（エラー）へ遷移し、関数を終了する。
3. setBuzzer(true) を呼び、休憩開始通知を行う（buzzerMs は setBuzzer 内で記録される）。
4. breakStartMs = millis() を保存し、休憩タイマーの基準時刻にする。
5. printLog("BREAK_START") を実行し、state=1（休憩中）へ遷移する。

【エラー・異常ケース】
- startTime.year < 2000 の場合: RTC未設定などの異常とみなし、state=3（エラー）へ遷移する。
```

---

### `endBreak()` — 終了時刻を記録し待機中へ遷移する

**basic_design.md 2-2 との対応：** 終了時刻を記録し待機中へ遷移

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. endTime = clock.getDateTime() でRTCから直接終了時刻を取得する。
2. endTime.year < 2000 の場合は state=3（エラー）へ遷移し、関数を終了する。
3. setBuzzer(false) を呼び、通知中であればブザーを停止する。
4. printLog("BREAK_END") を実行し、state=0（待機中）へ遷移する。

【エラー・異常ケース】
- endTime.year < 2000 の場合: RTC未設定などの異常とみなし、state=3（エラー）へ遷移する。
```

---

### `printLog(const char* msg)` — シリアルモニタに時刻ログを出力する

**basic_design.md 2-2 との対応：** シリアルモニタに時刻を出力

**引数：** `msg`（`const char*`）: 出力メッセージ種別

**戻り値：** `void`

```
【処理の流れ】
1. msg が "BREAK_START" の場合:
   - startTime を "YYYY/MM/DD HH:MM:SS" 形式で出力する。
2. msg が "BREAK_END" の場合:
   - endTime を "YYYY/MM/DD HH:MM:SS" 形式で出力する。
   - state が 1（休憩中）の場合は "(早期終了)" を追加で出力する。
3. その他の msg の場合:
   - msg をそのまま出力する。

【エラー・異常ケース】
- シリアル出力失敗時: 再送を試みる処理は実装されていないため、特に考慮しない。
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
  また、ブザー制御では「ブザー開始時のmillis（buzzerMs）」を記録し、「現在のmillis() - buzzerMs >= 1000」で1秒経過を判定する。

【処理の流れ（例: 休憩タイマー）】
  1. 休憩開始時に breakStartMs = millis() を記録
  2. 毎ループで now = millis() を取得
  3. now - breakStartMs >= duration*1000 なら休憩終了（通知開始）
  4. それ以外は休憩継続

【処理の流れ（例: ブザー制御）】
  1. ブザーON時に buzzerMs = millis() を記録
  2. 毎ループで now = millis() を取得
  3. buzzer == true かつ now - buzzerMs >= 1000 なら setBuzzer(false) を呼び出しブザー停止
  4. それ以外はブザー鳴動を継続

【自分のシステムで millis() を使う処理】
  - 休憩時間の経過判定（breakStartMs, duration, now）
  - ボタンのデバウンス判定（btnLastMs, debounceMs, now）
  - ブザーの1秒非同期制御（buzzerMs, now）
  - 必要に応じて他のタイミング管理にも応用可能
```

---


## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | ボタン押下イベントが正しく検出されているか | `readButton()` | `Serial.println("Button event detected");` |
| 2 | 状態遷移が正しく行われているか | `loop()` | `Serial.println("State: "); Serial.println(state);` |
| 3 | ブザーの1秒非同期制御が正しく動作しているか | `setBuzzer()` | `Serial.println("Buzzer state: ON");` または `Serial.println("Buzzer state: OFF");` |
| 4 | RTCから時刻が正しく取得できているか | `getTime()` | `Serial.println("RTC Time: "); Serial.println(currentTime);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | ボタンを1回押す | true が返る | trueを確認 | [〇] |
| 2 | readButton() | ボタンを素早く2回押す | 1回分だけ true になる | 一回分のみ | [〇] |
| 3 | readButton() | ボタンを長押しする | true が返る（1回分のみ） | 一回分のみ | [〇] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | setBuzzer(true) | ブザーをONにする | ブザーが鳴動し始める |鳴動する| [〇] |
| 2 | setBuzzer(false) | ブザーをOFFにする | ブザーが停止する |停止する| [〇] |
| 3 | printLog("BREAK_START") | 休憩開始時のログを出力 | シリアルモニタに"BREAK_START"と時刻が出力される |出力される| [〇] |
| 4 | printLog("BREAK_END") | 休憩終了時のログを出力 | シリアルモニタに"BREAK_END"と時刻が出力される |出力される| [〇] |
| 5 | printLog() | 開始/終了ログの時刻文字列を確認する | YYYY/MM/DD HH:MM:SS 形式で出力される |出力を確認| [〇] |
| 6 | printLog() | エラー発生直後にログを出力する | 警告メッセージと状態（state）が出力される |出力を確認(RTC_ERROR)| [〇] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 休憩開始時の1秒ブザー制御 | startBreak() 実行後、1秒経過を確認する | 約1秒後に setBuzzer(false) が自動実行されブザー停止 |1秒鳴動| [〇] |
| 2 | 通知中の停止条件（ボタン） | state=2（通知中）でボタンを押す | setBuzzer(false) と endBreak() が実行され state=0 へ戻る |ブザーが止まり待機中に遷移| [〇] |
| 3 | 休憩タイマー境界（-1ms） | breakStartMs から duration*1000-1ms 時点を確認 | state=1（休憩中）のまま |ボタンが押される前まで休憩中| [〇] |
| 4 | 休憩タイマー境界（ちょうど） | breakStartMs から duration*1000ms 時点を確認 | state=2（通知中）へ遷移し通知開始 |遷移するが記録に誤差±5秒| [△] |
| 5 | millis()タイマーの精度 | 休憩時間を計測し、ストップウォッチで確認 | 設定した休憩時間通りに動作する |タイマーとしては精度高め | [〇] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- 休憩満了時の通知ブザーが自動停止しない設計になっている
-  INPUT_PULLUPでは未押下=HIGHですが、btnStable初期値がfalse（実装上LOW相当）だと、初回読取時に状態変化と誤解して押下エッジ判定が乱れるリスクがあります。
- getTime失敗時の復帰動作が関数間で不整合
- 状態遷移の責務が二重化している
- エラー状態の入口が明確でない

**対応した内容：**
- loopのstate2に「押下で停止」を停止条件を追加
- setupで1回 !digitalRead
- endTime = 0の時も待機中に戻すように修正
- 関数のみとする
- 1回失敗で state=3

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 主要機能のテストはあるが、getTime失敗時の state=3 遷移確認が不足。
- デバウンスの境界値（49/50/51ms）と休憩タイマーの境界値（duration*1000-1ms / ちょうど）を追加すべき。
- setup直後の誤押下イベント防止、printLogの時刻フォーマット確認も必要。

**対応した内容：**
- 5-1 に setup初期同期テスト、getTime失敗時の state=3 遷移テスト、デバウンス境界値テストを追加。
- 5-2 に printLogの時刻フォーマット確認テストを追加。
- 5-3 に休憩タイマー境界値テストと通知中停止条件テストを追加。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | loop()内にエラー時の動作が無い | 呉 | 元々電源を抜いて解決しようと思っていたけど、エラー時の動作を書いた方がいいかもしれないです |

### 7-2. レビューを受けて変更した点

- エラー処理を追加
- コメントで日本語で説明を加えた

---

*初版: 2026-05-25 / AIレビュー: 2026-05-26 / グループレビュー後更新: 2026-05-25*
