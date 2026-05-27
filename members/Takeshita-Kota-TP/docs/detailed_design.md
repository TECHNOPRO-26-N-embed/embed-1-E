# 詳細設計書 — 組込み開発実習

<!-- 作成者: 竹下倖詩 / 日付: 2026-05-25 / グループ: 1-E -->

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
| 作品タイトル | 反応速度計測マシーン |
| 状態の種類（1-2 状態遷移から） | 待機、計測、結果表示、 |
| 実装する関数の数（2-2 関数一覧から） | 8個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 29B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_BUTTON_1   = 2    // タクトスイッチ（INPUT_PULLUP）
  PIN_LED    = 13(LED_BUILTIN)    // LED
  PIN_BUZZER_ACTIVE    = 12   // アクティブブザー
  PIN_BUTTON_2 = 3   // タクトスイッチ（INPUT_PULLUP）
  PIN_BUZZER_PASSIVE    = 11   // パッシブブザー
  letch    = 8（DS）   // 4桁の7セグメントディスプレイ
  clock    = 9（STCP）   // 4桁の7セグメントディスプレイ
  data    = 10（SHCP）   // 4桁の7セグメントディスプレイ\
  digitPins[4] = {4,5,6,7}    // 各桁の制御ピン

【状態管理】（basic_design.md 1-2 の状態名から転記）
  // currentState を持たずに、処理位置と判定結果で状態を管理する。
  isMeasuring   : bool = false   // false:待機 / true:計測中
  result_number : int = -1       // -1:未判定 0:失敗 1:成功 2:エラー(タイムオーバー)
  // 実質の遷移: 待機 -> 計測中 -> 結果表示 -> 待機
  // 例外: 計測中にTIMEOUT_MS超過で待機へ戻る


【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  start_time              : unsigned long = 0   // LED+アクティブブザーON時刻
  measure_time            : unsigned long = 0   // 計測ループ内の現在時刻
  end_time                : unsigned long = 0   // 計測ボタン押下時刻
  total_time              : unsigned long = 0   // 反応時間（end_time - start_time）
  keika_time              : unsigned long = 0   // 計測開始からの経過時間
  lastChangeTime_start    : unsigned long = 0   // 開始ボタンのデバウンス用
  lastChangeTime_measure  : unsigned long = 0   // 計測ボタンのデバウンス用
  DEBOUNCE_MS             : const unsigned long = 50
  SIGNAL_ON_MS            : const unsigned long = 100
  TIMEOUT_MS              : const unsigned long = 20000


【センサー・入力値】（basic_design.md 2-1 から転記）
  button_state : bool = false   // true:オン false:オフ
  led_state : void
  active_state : void
  start_time : unsigned long
  button_measure : bool = false   // true:押下 false:未押下
  end_time : unsigned long
  total_time : unsigned long
  suc_fail : int = 0,1,2  // 0:失敗 1:成功 2:エラー
  passive_state : void  // true:鳴動 false:停止
  nums[3] : int  // total_timeの各桁の数字が配列として格納される
  table[] : unsigned char  //7セグメントディスプレイに表示する値
  debounceDelay　: const unsgined long

【その他のフラグ・カウンター】
  （自分のものを追加）
```
---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---
### `setup()` — 初期化処理

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. 各種ピンモードを設定する。
  - PIN_BUTTON_1, PIN_BUTTON_2 → INPUT_PULLUP
  - PIN_LED, PIN_BUZZER_ACTIVE, PIN_BUZZER_PASSIVE → OUTPUT
2. 7セグメントディスプレイ（TM1637）を初期化する。
  - display = TM1637Display(CLK, DIO)
  - display.setBrightness(0x0f)
  - 初期表示をクリアする
3. randomSeed(analogRead(0)) で乱数シードを設定する。
4. デバッグ用のSerial.begin(9600);を設定
5. 変数を初期値にそろえる。
  - currentState = 0（待機）
```
---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 各関数の動作
  - random(0,10000)で開始ボタンを押してから、LEDとアクティブブザーが動作するまでのランダムな待機時間を生成

＜suc_fail が 失敗 のとき＞
  - buzzer_state = 0にする
  - 4桁の7セグメントディスプレイに計測結果を表示する

＜suc_fail が 成功 のとき＞
  - buzzer_state = 1にする
  - 4桁の7セグメントディスプレイに計測結果を表示する

＜suc_fail が エラー のとき＞
  - 4桁の7セグメントディスプレイに----を表示する
  - 待機中へ戻る

＜計測結果 が 失敗 のとき＞
  - suc_failで0を返し、passive_stateで失敗音を鳴らす

＜計測結果 が 成功 のとき＞
  - suc_failで1を返し、passive_stateで成功音を鳴らす
```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。
---
### `randomSeed(analogRead(0))` — ランダム値の均一化を防ぐ

**basic_design.md 2-2 との対応：** random()で値を生成する際に値が固定されるのを防ぐ

**引数：** `なし`

**戻り値：** なし

---
### `button_start()` — 開始ボタン動作

**basic_design.md 2-2 との対応：** 開始ボタンを押す

**引数：** `int start_button`

**戻り値：** bool型のtrueもしくはfalse

```
【処理の流れ】
1. digitalReadで開始ボタンが押されたかどうかを取得
2. millisでチャタリング対策
3. LOWになった時にtrue、そうでない時はfalseを返す

【エラー・異常ケース】
- 異常な値が来た場合:
　・
```
---
### `random_number()` — ランダムな値生成

**basic_design.md 2-2 との対応：** ランダムな値生成

**引数：** `int number_start, int number_end `

**戻り値：** int型

```
【処理の流れ】
1. random(num_st, num_end)でnum_st以上num_end以下の適当な値を生成し、numberに格納
2. 生成した値がnum_endより大きい限りwhileループ

【エラー・異常ケース】
- 異常な値が来た場合:
　・while文で条件判別を行う
```
---
### `led_active_state()` — LEDとアクティブブザーの同時動作

**basic_design.md 2-2 との対応：** LEDとアクティブブザーの同時動作

**引数：** `int led, int active, bool state`

**戻り値：** unsigned long型
```
【処理の流れ】
1. random_number()関数の結果をint time_waitに格納し、ボタンを誤って押すことでもう一度動作することが無いよう、delay(time_wait)を行う
2. LEDはdigitalWrite、アクティブブザーはanalogWriteで同時に動作
3. LED・アクティブブザーが動作した瞬間をstart_timeにmillis()で計測
4. millis()で100ms待ち、LEDとアクティブブザーの動作を止める(LEDとアクティブブザーを動作させて、ms決定予定)

【エラー・異常ケース】
- LEDやアクティブブザーが動作しない時:
　・ardiuno unoとpcの接続を切り、LED・アクティブブザー個別で動作確認を行う。
```
---
### `button_measure()` — 計測ボタン動作

**basic_design.md 2-2 との対応：** 計測ボタンを押す

**引数：** `int measure_button, unsigned long &end_time`

**戻り値：** bool型

```
【処理の流れ】
1. digitalReadで計測ボタンが押されたかどうかを取得
2. 計測ボタンを押した瞬間をint end_timeに計測
3. millisでチャタリング対策

【エラー・異常ケース】
- 10000ms(10.000秒)以上経過しても計測ボタンが押されなかった場合:
  ・待機中に戻る。
```
---
### `suc_fail()` — 計測結果の計算・判定

**basic_design.md 2-2 との対応：** 計測結果を計算し、成功か失敗の判定を行う

**引数：** `unsigned long time_start, unsigned long time_end`

**戻り値：** int型

```
【処理の流れ】
1. time_end(button_measure()で得られるend_time) - time_start(led_active_stateで得られるstart_time)を行い、結果をtotal_timeに格納。
2. if文判定の結果を格納するint paternを作る
3. switch文を使用し、total_timeが433ms(0.433秒)以下ならpatern = 1。
　 434ms(0.434秒)以上ならpatern = 0
　 10000ms(10.000秒)以上の場合はエラーとしてpatern = 2として、戻り値を出す。
```
---
### `passive_state()` — パッシブブザー動作

**basic_design.md 2-2 との対応：** パッシブブザーで成功・失敗音が鳴る

**引数：** `int result_number`

**戻り値：** bool型

```
【処理の流れ】
1. if文で、suc_fail()の戻り値が1なら成功音を鳴らす
2. 0か2を戻り値として持つなら失敗音を鳴らす

【エラー・異常ケース】
- 動作しない時:
　・ardiuno unoとpcの接続を切り、個別で動作確認を行う。
```
---
### `seg_47()` — 計測結果の表示

**basic_design.md 2-2 との対応：** 4桁の7セグメントディスプレイに結果を表示

**引数：** `unsigned long total_time, int aftet`

**戻り値：** void型

```
【処理の流れ】
1. 結果に応じてtotal_timeを4桁の7セグメントディスプレイに表示する
2. 10000ms以上の場合は----と表示

【エラー・異常ケース】
- 結果が10000ms(10.000秒)以上の時:
  ・----と表示
```
---
### `Display()` — 1桁分の点灯

**basic_design.md 2-2 との対応：** 4桁7セグの1桁あたりの表示方法(ダイナミック方式)

**引数：** `unsigned long total_time, int aftet`

**戻り値：** void型

```
【処理の流れ】
1. 74HC595への出力をストップ
2. table[nums]から対応した表示パターンを取得
3. 対象の部分のDP(小数点部分)を点灯させる
4. 74HC595への出力を行う

```
---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms以内の連続入力は「同じ1回の押下」として無視する。
  複数ボタンがある場合は、それぞれ独立してデバウンス処理を行う。

【処理の流れ】
  1. ボタン入力を `digitalRead()` で取得し、`readingX`（今回値）として扱う。
  2. `readingX` と `lastReadingX` が異なる場合、チャタリング中の可能性があるため `lastChangeTimeX = millis()` に更新し、`lastReadingX = readingX` にする。
  3. `millis() - lastChangeTimeX >= DEBOUNCE_MS`（50ms）になるまで確定判定を行わず待つ。
  4. 50ms経過後、`stableStateX`（確定状態）と `readingX` が異なる場合のみ状態変化として確定する。
  5. 確定後に `stableStateX = readingX` とし、`stableStateX == LOW`（押下）なら「1回押された」と判定して true を返す。
  6. 上記条件を満たさない場合は false を返し、次のループで再判定する（開始ボタン・計測ボタンそれぞれ独立して同じ処理を持つ）。

【必要な変数（Section 1 に追加済みか確認）】
  stableState_start     : int = HIGH            // 開始ボタンの確定状態
  lastReading_start     : int = HIGH            // 開始ボタンの前回読取値
  lastChangeTime_start  : unsigned long = 0     // 開始ボタンの変化時刻
  stableState_measure   : int = HIGH            // 計測ボタンの確定状態
  lastReading_measure   : int = HIGH            // 計測ボタンの前回読取値
  lastChangeTime_measure: unsigned long = 0     // 計測ボタンの変化時刻
  DEBOUNCE_MS           : const unsigned long = 50  // チャタリング判定時間（ms）

【補足】
  - 状態遷移（loop内）で「押下イベント」として使う場合は、デバウンス済みの値を参照すること。
  - 本コードではbutton_start()とbutton_measure()で、それぞれ独立したデバウンス状態として管理する。
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  1 ボタン入力のデバウンス（50ms）
  2 LED+アクティブブザーをONにした時刻の記録（start_time）
  3 計測中の経過時間監視（タイムアウト判定）
  ※ 本実装は delay() も併用しているため、完全な非停止型ではない。

【処理の流れ】
  1. 開始ボタン確定後、led_active_state() でランダム待機後にLEDとアクティブブザーをONにする。
  2. ONにした瞬間を start_time = millis() で記録する。
  3. while (millis() - start_time < 100) で100ms経過を待ち、LEDとアクティブブザーをOFFにする。
  4. loop() 側で measure_time = millis() を取得し、keika_time = measure_time - start_time を毎ループ計算する。
  5. 計測ボタン押下が確定したら、end_time = millis() を取得して total_time = end_time - start_time を算出する。
  6. keika_time >= 20000 になった場合はタイムアウトとして待機へ戻る。

【自分のシステムで millis() を使う処理例】
  - 開始ボタン/計測ボタンのデバウンス判定（50ms）
  - 反応計測の開始時刻記録（start_time）
  - 計測ボタン押下時刻記録（end_time）
  - 計測中の経過時間監視（keika_time）
  - タイムアウト判定（20000ms）

【必要な変数（Section 1 に追加済みか確認）】
  start_time              : unsigned long   // LED+アクティブブザーON時刻
  end_time                : unsigned long   // 計測ボタン押下時刻
  total_time              : unsigned long   // 反応時間（end_time - start_time）
  measure_time            : unsigned long   // 計測ループ内の現在時刻
  keika_time              : unsigned long   // 計測開始からの経過時間
  lastChangeTime_start    : unsigned long   // 開始ボタンのデバウンス用
  lastChangeTime_measure  : unsigned long   // 計測ボタンのデバウンス用
  DEBOUNCE_MS             : const unsigned long = 50
  SIGNAL_ON_MS            : const unsigned long = 100
  TIMEOUT_MS              : const unsigned long = 20000

【補足・注意点】
  - millis() は差分計算（現在時刻 - 記録時刻）で判定しているため、オーバーフロー時も基本的に安全に動作する。
  - コメントでは「30秒以内」とあるが、実装値は 20000ms（20秒）になっている。仕様値をどちらかに統一する。
  - delay() を使っている箇所（ランダム待機・効果音）はその間ほかの処理が止まるため、必要なら millis() 管理へ置換する。
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

1. **型宣言・スペルミス**
  - `unsgined` → `unsigned` に修正が必要です。
  - `unsigned float`型は存在しません。`float`型のみ使用してください。
  - `passive_state : void` → 変数に`void`型は使えません。boolやint等に修正。
  - `randonmSeed` → `randomSeed`が正しいです。
  - `measuer_millis` → `measure_millis`が正しいです。

2. **チャタリング対策**
  - デバウンス用変数（`lastDebounceTimeX`等）がSection 1に全て追加されているか再確認。
  - `DEBOUNCE_DELAY`は`const int`で宣言し、50ms等の値を設定してください。

3. **タイマー管理**
  - `millis()`を使う際は`unsigned long`型で管理し、差分計算（`now - lastMillis_X`）を徹底。

4. **関数の戻り値・引数**
  - `suc_fail()`の戻り値はfloat型ですが、成功/失敗/エラーの判定ならint型やenum型が適切です。
  - `random_number()`の引数名・型が曖昧です。`int num_st, int num_end`のように明記しましょう。

5. **状態管理**
  - `currentState`や`button_state`など、複数の状態変数が混在しているため、状態遷移図と実装が一致しているか再確認。

6. **未初期化変数・初期値**
  - 変数の初期値が明記されていないものは、必ず`setup()`で初期化してください。

7. **論理値の扱い**
  - `bool`型の変数は`true/false`で統一し、0/1で扱わないよう注意。

8. **7セグメント表示**
  - 4桁の7セグメントで表示できる値の範囲を超えた場合（例：10000ms以上）は`----`表示などの例外処理を必ず実装。

9. **例外・異常系の処理**
  - 各関数の「エラー・異常ケース」について、実装時に必ず分岐を入れてください。

10. **その他**
  - 変数・関数名の一貫性を保つこと。
  - デバッグ用の`Serial.println()`は不要になったら削除すること。

**対応した内容：**
（例：型宣言の修正、初期値の明記、異常系分岐の追加、関数名の統一などを実装時に反映）

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
| 1 | 確認でランダムシートを使うことで毎回で乱数を出るんですか | 神谷 | そうです。 |
| 2 | button_start()に月に押す際はオフになるように！で反転処理はなぜそうするんですか？ | 神谷 | 感覚的に一回押したらオンになってる方が分かりやすいかなって感じ＋押している間ずっとにするとボタンを一瞬離したらそこで強制的にやり直しになってしまう |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*