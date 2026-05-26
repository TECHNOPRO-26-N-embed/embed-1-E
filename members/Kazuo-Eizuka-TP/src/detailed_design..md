# 詳細設計書 — 組込み開発実習

<!-- 作成者: Kazuo Eizuka / 日付: 2026-05-26 / グループ: 1-E -->

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
| 作品タイトル | カウントダウンタイマー |
| 状態の種類（1-2 状態遷移から） | 初期化 / 待機中 / 時間設定中 / カウントダウン中 / 一時停止中 / 警告中 |
| 実装する関数の数（2-2 関数一覧から） | 13個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 20B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_BTN_SET      = 2   // 時間設定ボタン（INPUT_PULLUP）
  PIN_BTN_START    = 3   // 開始/一時停止ボタン（INPUT_PULLUP）
  PIN_BTN_RESET    = 4   // リセットボタン（INPUT_PULLUP）
  PIN_TM1637_CLK   = 6   // 4桁7セグ CLK
  PIN_TM1637_DIO   = 7   // 4桁7セグ DIO
  PIN_BUZZER       = 12  // Active Buzzer

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState     : int = 0   // 0:初期化 1:待機中 2:時間設定中 3:カウントダウン中 4:一時停止中 5:警告中

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastButtonScanMs  : unsigned long = 0   // 10ms周期入力監視
  lastCountTickMs   : unsigned long = 0   // 1000ms周期減算
  lastDisplayMs     : unsigned long = 0   // 50ms周期表示更新
  lastBlinkMs       : unsigned long = 0   // 500ms周期点滅
  warnStartMs       : unsigned long = 0   // 警告開始時刻

【センサー・入力値】（basic_design.md 2-1 から転記）
  timerValueSec    : unsigned int = 60    // 設定時間（秒）初期1分
  remainingSec     : unsigned int = 60    // 残り時間（秒）
  buttonStable[3]  : bool = {false, false, false}
  buttonPrevRaw[3] : bool = {false, false, false}
  lastDebounceMs[3]: unsigned long = {0, 0, 0}
  displayBuffer[6] : char = "00:00"

【その他のフラグ・カウンター】
  buzzerOn         : bool = false
  blinkVisible     : bool = true
  longPressStartMs : unsigned long = 0
  isLongPressUsed  : bool = false

【定数】
  DEBOUNCE_DELAY_MS    : const unsigned long = 50
  BUTTON_SCAN_MS       : const unsigned long = 10
  COUNT_INTERVAL_MS    : const unsigned long = 1000
  DISPLAY_INTERVAL_MS  : const unsigned long = 50
  BLINK_INTERVAL_MS    : const unsigned long = 500
  BUZZER_LIMIT_MS      : const unsigned long = 30000
  LONG_PRESS_MS        : const unsigned long = 800
  MIN_TIMER_MIN        : const unsigned int = 1
  MAX_TIMER_MIN        : const unsigned int = 99
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
  - D2, D3, D4 を INPUT_PULLUP
  - D12 を OUTPUT（初期LOW）
  - TM1637（D6, D7）を初期化
2. 変数を初期化する
  - currentState = 初期化
  - timerValueSec = 60, remainingSec = 60
  - 各種 last*Millis を millis() で初期化
3. 起動シーケンスを実行する
  - 7セグに "00:00" を表示して配線確認
  - 必要なら100msだけブザー確認音を鳴らす
  - currentState を 待機中 へ遷移
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
  - nowMs = millis() を取得
  - getButton() でボタンイベントを取得
  - showTime(remainingSec) を50ms周期で更新
  - buzzControl(nowMs) を実行（警告中の鳴動管理）

＜currentState が 初期化（0）のとき＞
  - setup()完了後は通常入らない想定
  - 入った場合は安全のため待機中へ戻す

＜currentState が 待機中（1）のとき＞
  - setTime(ev) で設定値変更を受け付ける
  - 開始ボタン押下で remainingSec = timerValueSec
  - currentState = カウントダウン中 へ遷移

＜currentState が 時間設定中（2）のとき＞
  - setTime(ev) を継続実行
  - 一定時間入力がなければ待機中へ戻す
  - 開始ボタン押下でカウントダウン中へ遷移

＜currentState が カウントダウン中（3）のとき＞
  - updateTimer(nowMs) で1秒減算
  - togglePause(ev) で一時停止へ切替
  - remainingSec == 0 なら buzzOnTimeout() 実行

＜currentState が 一時停止中（4）のとき＞
  - togglePause(ev) でカウントダウン再開
  - resetTimer(ev) で設定状態へ復帰

＜currentState が 警告中（5）のとき＞
  - blinkDisp(nowMs) で表示点滅
  - resetTimer(ev) で待機中に戻る
  - buzzControl(nowMs) が30秒経過でブザー停止

＜全状態共通＞
  - resetボタンが押されたら resetTimer(ev) を優先実行

```

---

### `getButton()` — 3ボタン入力をデバウンスしてイベント化する

**basic_design.md 2-2 との対応：** 3つのボタン入力を読み取り、デバウンス後の押下イベントを返す

**引数：** なし

**戻り値：** `ButtonEvent`（NONE / SET_SHORT / SET_LONG / START_PRESS / RESET_PRESS）

```
【処理の流れ】
1. 10ms周期で D2, D3, D4 の生入力を取得する（押下時LOW）。
2. 各ボタンで「生入力が前回と変化したか」を確認し、変化時刻を更新する。
3. 変化後50ms以上継続していれば安定入力として確定する。
4. 未押下→押下の立ち下がりエッジのみイベントを生成する。
5. D2は押下継続時間を見て短押し/長押しを判定する。

【エラー・異常ケース】
- 同一ボタンで50ms未満の再入力はチャタリングとして無視する。
```

---

### `setTime(ButtonEvent ev)` — 設定時間を1〜99分の範囲で変更する

**basic_design.md 2-2 との対応：** 時間設定ボタン操作で設定時間を増減し範囲内に収める

**引数：** `ev`（ButtonEvent）: 押下種別

**戻り値：** void

```
【処理の流れ】
1. ev が SET_SHORT なら +1分、SET_LONG なら +10分を加算する。
2. 分単位の設定値が99分を超える場合は99分にクリップする。
3. 設定値を秒に変換し timerValueSec と remainingSec を同期する。
4. currentState を時間設定中へ遷移させる。

【エラー・異常ケース】
- 無効イベント（NONE など）は何もしない。
```

---

### `togglePause(ButtonEvent ev)` — カウントダウンと一時停止を切り替える

**basic_design.md 2-2 との対応：** スタート/一時停止ボタン入力で状態を切り替える

**引数：** `ev`（ButtonEvent）: 開始/停止ボタン押下

**戻り値：** void

```
【処理の流れ】
1. ev が START_PRESS でなければ処理を終了する。
2. currentState がカウントダウン中なら一時停止中へ遷移する。
3. currentState が一時停止中なら、lastCountTickMs を nowMs で更新して再開する。

【エラー・異常ケース】
- 警告中や待機中で誤って呼ばれても状態を壊さないよう無視する。
```

---

### `updateTimer(unsigned long nowMs)` — 1秒周期で残り時間を減算する

**basic_design.md 2-2 との対応：** millis()ベースで1秒経過を判定し、残り時間を更新する

**引数：** `nowMs`（unsigned long）: 現在時刻

**戻り値：** bool（1秒更新したら true）

```
【処理の流れ】
1. nowMs - lastCountTickMs >= 1000 を確認する。
2. 条件成立かつ remainingSec > 0 のとき remainingSec を1減算する。
3. lastCountTickMs を nowMs で更新する。
4. remainingSec == 0 なら true を返し、上位で buzzOnTimeout() を呼ぶ。

【エラー・異常ケース】
- 残り時間0未満にならないよう unsigned のまま減算ガードを入れる。
```

---

### `showTime(unsigned int seconds)` — 残り時間をMM:SS表示へ反映する

**basic_design.md 2-2 との対応：** 現在の状態と残り時間をMM:SS形式で7セグ表示に反映する

**引数：** `seconds`（unsigned int）: 表示対象秒数

**戻り値：** void

```
【処理の流れ】
1. minutes = seconds / 60, sec = seconds % 60 を計算する。
2. displayBuffer を "MM:SS" 形式で生成する。
3. 50ms周期でTM1637へ表示データを書き込む。

【エラー・異常ケース】
- 99:59を超える値は99:59で表示固定する。
```

---

### `buzzOnTimeout()` — 時間切れ時に警告状態へ遷移する

**basic_design.md 2-2 との対応：** 残り時間が0になった時に警告状態へ遷移しブザー通知を開始する

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. currentState = 警告中 に設定する。
2. buzzerOn = true にして D12 を HIGH 出力する。
3. warnStartMs = millis() を保存する。

【エラー・異常ケース】
- 二重呼び出し時は warnStartMs を初回値で保持し続ける。
```

---

### `buzzControl(unsigned long nowMs)` — 警告中ブザーの30秒自動停止を管理する

**basic_design.md 2-2 との対応：** 警告中のブザーON/OFFと30秒自動停止を制御する

**引数：** `nowMs`（unsigned long）: 現在時刻

**戻り値：** void

```
【処理の流れ】
1. currentState が警告中でない場合は何もしない。
2. nowMs - warnStartMs >= 30000 なら D12 をLOWにして buzzerOn=false。
3. ブザー停止後も表示点滅は継続する。

【エラー・異常ケース】
- millis() オーバーフロー時も差分計算で判定可能な実装にする。
```

---

### `resetTimer(ButtonEvent ev)` — リセット操作で設定状態へ戻す

**basic_design.md 2-2 との対応：** リセット操作でタイマー値と状態を初期化し設定状態へ戻す

**引数：** `ev`（ButtonEvent）: リセット押下イベント

**戻り値：** void

```
【処理の流れ】
1. ev が RESET_PRESS のときのみ処理する。
2. remainingSec = timerValueSec に戻す。
3. ブザー停止、点滅フラグ初期化を行う。
4. currentState = 待機中 に遷移する。

【エラー・異常ケース】
- どの状態から呼ばれても同じ初期復帰手順で安全に戻す。
```

---

### `presetTime(ButtonEvent ev)` — 3/5/10分のプリセット時間を設定する

**basic_design.md 2-2 との対応：** プリセット入力に応じて設定時間を3/5/10分へ変更する

**引数：** `ev`（ButtonEvent）: プリセット選択イベント

**戻り値：** void

```
【処理の流れ】
1. プリセット選択イベントに応じて 180/300/600秒を設定する。
2. timerValueSec と remainingSec を同期する。
3. currentState を時間設定中へ更新する。

【エラー・異常ケース】
- 未定義プリセット値は反映せず、直前値を保持する。
```

---

### `blinkDisp(unsigned long nowMs)` — 警告中の表示を点滅させる

**basic_design.md 2-2 との対応：** 警告中にボタン入力があるまで表示を点滅させる

**引数：** `nowMs`（unsigned long）: 現在時刻

**戻り値：** void

```
【処理の流れ】
1. nowMs - lastBlinkMs >= 500 を確認する。
2. blinkVisible を反転し、trueなら "00:00"、falseなら消灯表示を出す。
3. lastBlinkMs = nowMs を更新する。

【エラー・異常ケース】
- 警告中以外で呼ばれた場合は何もしない。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastDebounceTime）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（例: 50ms）→ 無視する
  4. 経過時間 ≥ DEBOUNCE_DELAY → ボタンの状態として確定する
  5. lastDebounceTime を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceMs[3] : unsigned long            // ボタンごとの前回確定時刻
  buttonPrevRaw[3]  : bool                     // ボタンごとの前回生入力
  buttonStable[3]   : bool                     // ボタンごとの確定状態
  DEBOUNCE_DELAY_MS : const unsigned long = 50 // チャタリング判定時間
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  1) ボタン入力監視: 10ms周期
    - nowMs - lastButtonScanMs >= 10 で getButton() 実行
  2) カウントダウン減算: 1000ms周期
    - nowMs - lastCountTickMs >= 1000 で remainingSec を1減算
  3) 7セグ表示更新: 50ms周期
    - nowMs - lastDisplayMs >= 50 で showTime() 実行
  4) 警告表示点滅: 500ms周期
    - nowMs - lastBlinkMs >= 500 で blinkDisp() 実行
  5) ブザー自動停止判定: 10ms周期
    - 警告開始から30000ms経過を buzzControl() で判定
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 時間設定の長押し判定
  - D2押下継続が800ms以上なら SET_LONG を生成
  - それ未満で離した場合は SET_SHORT を生成
2. 警告中の表示点滅停止条件
  - リセット押下で点滅を終了し待機状態へ戻す
3. 1〜99分クリップ処理
  - setTime() と presetTime() の両方で範囲外入力を丸める

【入力値と出力値の関係】
  - 入力: SET_SHORT      → 出力: 設定時間 +1分
  - 入力: SET_LONG       → 出力: 設定時間 +10分
  - 入力: START_PRESS    → 出力: 状態切替（開始/一時停止）
  - 入力: RESET_PRESS    → 出力: 待機中へ復帰・ブザー停止
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | ボタンイベントが正しく検出できるか | `getButton()` | `Serial.println(ev);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | 1秒周期で減算できているか | `updateTimer()` | `Serial.println(remainingSec);` |
| 4 | ブザー自動停止時刻が正しいか | `buzzControl()` | `Serial.println(nowMs - warnStartMs);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `getButton()` | D2（時間設定）を1回押す | `SET_SHORT` が1回だけ返る | | [ ] |
| 2 | `getButton()` | D2を50ms未満で連続押下する | 1回分のみ有効、残りは無視される | | [ ] |
| 3 | `getButton()` | D2を800ms以上押し続ける | `SET_LONG` が返る（+10分相当の入力） | | [ ] |
| 4 | `setTime()` | 98分状態で `SET_LONG` を入力 | 99分でクリップされ、100分以上にならない | | [ ] |
| 5 | `togglePause()` | カウント中にD3を押す | 状態が「一時停止中」に遷移する | | [ ] |
| 6 | `resetTimer()` | 任意状態でD4を押す | 待機中へ戻り、残り時間が設定値に戻る | | [ ] |

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
- 状態遷移の優先順位に注意。`RESET_PRESS` を全状態で最優先しないと、警告中や一時停止中で意図しない分岐が残る可能性がある。
- `togglePause()` の再開時は `lastCountTickMs` を現在時刻へ更新しないと、再開直後に1秒以上まとめて減算されたように見える場合がある。
- `unsigned int` の残り時間は 0 未満にできないため、減算前に `remainingSec > 0` を必ず確認する。
- `millis()` 比較は差分演算（`now - last >= interval`）に統一し、オーバーフロー耐性を維持する。
- TM1637表示更新を高頻度にし過ぎると処理負荷が増えるため、50ms周期を上限目安として維持する。

**対応した内容：**
- loop設計に「全状態共通で resetTimer(ev) を優先実行」を明記した。
- `togglePause()` の再開時に `lastCountTickMs` 更新を設計へ追記した。
- `updateTimer()` に減算ガード（`remainingSec > 0`）を明記した。
- すべての周期処理で `millis()` 差分判定を使う方針を Section 3-2 に統一した。
- 表示更新周期を 50ms のまま固定し、不要な高頻度更新を避ける方針にした。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系テストは主要イベントを押さえているが、境界値（1分下限・99分上限）と時間切れ直前後の遷移確認を追加すると網羅性が上がる。
- 長押し判定は「しきい値ちょうど（800ms）」のテストが必要。799ms/800msで判定が分かれることを確認すべき。
- 出力系では `showTime()` の書式（1桁分の0埋め）確認、警告中のブザー30秒自動停止、点滅周期500msの誤差確認が必要。
- タイミング系では、カウントダウン中にボタン操作しても表示更新と入力受付が止まらない並行動作テストを推奨。

**対応した内容：**
- 入力系テストに 99分クリップ確認（No.4）とリセット復帰（No.6）を含めた。
- 追加予定項目として、800ms境界値テスト（799ms/800ms）を 5-1 に追記予定とした。
- 5-2/5-3 を更新する際、MM:SSゼロ埋め、30秒自動停止、500ms点滅周期の検証を追加する方針を確定した。
- 統合動作確認として「表示更新・入力受付・減算の同時成立」を確認するテストを追加予定とした。

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
