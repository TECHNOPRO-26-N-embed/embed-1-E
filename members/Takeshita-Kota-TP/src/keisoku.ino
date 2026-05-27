const int PIN_BUTTON_1 = 2;  //開始ボタンピン番号
const int PIN_LED = LED_BUILTIN;  //LEDピン番号(13)
const int PIN_BUZZER_ACTIVE = 12;  //アクティブブザーピン番号
const int PIN_BUTTON_2 = 3;  //計測ボタンピン番号
const unsigned long DEBOUNCE_MS = 50;  //チャタリング判定の時間
const int PIN_BUZZER_PASSIVE = 11;  //パッシブブザーのピン番号
int latch=9;  //74HC595  pin 9 STCP
int clock=10; //74HC595  pin 10 SHCP
int data=8;   //74HC595  pin 8 DS
int digitPins[4] = {4, 5, 6, 7}; // 桁制御ピン（グローバル宣言）
unsigned char table[]=
{0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,
0x77,0x7c,0x39,0x5e,0x79,0x71,0x00
,0x40};//Display digital data

bool button_start(int start_button);  //開始ボタン動作
unsigned long led_active_state(int led, int active, bool state);  //LEDとアクティブブザーの同時動作
int random_number(int number_start, int number_end);  //ランダムな値生成
bool button_measure(int measure_button, unsigned long &end_time);  //計測ボタン動作
int suc_fail(unsigned long time_start, unsigned long time_end, unsigned long &total_time);  //成功/失敗判定
void passive_state(int result_number);  //パッシブブザー動作
void seg_47(unsigned long total_time, int after);  //4桁7セグへの結果表示
void Display(unsigned char num, bool dot);  //4桁7セグの1桁分の点灯

void setup(){
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER_ACTIVE, OUTPUT);
  pinMode(PIN_BUTTON_1, INPUT_PULLUP);
  randomSeed(analogRead(0));
  pinMode(PIN_BUTTON_2, INPUT_PULLUP);
  pinMode(PIN_BUZZER_PASSIVE, OUTPUT);
  pinMode(latch,OUTPUT);
  pinMode(clock,OUTPUT);
  pinMode(data,OUTPUT);
  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); // 初期状態OFF
  }
  Serial.begin(9600);
}

void loop(){
  static unsigned long last_total_time = 0;
  // 開始ボタンが押されたら新しい計測を開始
  if (button_start(PIN_BUTTON_1)){
    // すべての桁をOFFにする
    for (int k = 0; k < 4; k++) {
      digitalWrite(digitPins[k], HIGH); // HIGHがOFFの場合
    }
    // すべてのセグメントを消灯（table[16]が0x00なら）
    digitalWrite(latch, LOW);
    shiftOut(data, clock, MSBFIRST, 0x00); // 0x00で全消灯
    digitalWrite(latch, HIGH);
    unsigned long start_time = led_active_state(PIN_LED, PIN_BUZZER_ACTIVE, true);
    // 計測ボタンが30秒以内に押されなければ待機状態に戻る
    while (true) {
      unsigned long measure_time = millis();
      unsigned long keika_time = measure_time - start_time;
      unsigned long end_time;
      if (button_measure(PIN_BUTTON_2, end_time)) {
        unsigned long total_time;
        int result = suc_fail(start_time, end_time, total_time);
        Serial.print("result = ");
        Serial.println(result);
        passive_state(result);
        seg_47(total_time, result);
        break;
      }
      if (keika_time >=20000) {
        // 30秒経過したら待機状態（loopの先頭）に戻る
        Serial.println("time over");
        Serial.print("keika_time = ");
        Serial.println(keika_time);
        return;
      }
    }
  }
}

//開始ボタン動作関数
bool button_start(int start_button){
  static int stableState_start = HIGH;
  static int lastReading_start = HIGH;
  static unsigned long lastChangeTime_start = 0;
  int start_value = digitalRead(start_button);
  if (start_value != lastReading_start){
    lastChangeTime_start = millis();
    lastReading_start = start_value;
  }
  if (millis() - lastChangeTime_start >= DEBOUNCE_MS){
    if (stableState_start != start_value){
      stableState_start = start_value;
      if (stableState_start == LOW){
        return true;
      }
    }
  }
  return false;
}
//ランダムな値生成関数
int random_number(int number_start, int number_end){
  int number = random(number_start, number_end);
  while (number > number_end){
    number = random(number_start, number_end);
  }
  return number;
}
//LEDとアクティブブザーの同時動作関数
unsigned long led_active_state(int led, int active, bool state){
  if (!state){
    digitalWrite(led, LOW);
    analogWrite(active, 0);
    return 0;
  }
  int time_wait = random_number(0, 10000);
  Serial.print("time_wait = ");
  Serial.println(time_wait);
  delay(time_wait);
  digitalWrite(led, HIGH);
  analogWrite(active, 255);
  unsigned long start_time = millis();
  Serial.print("start_time = ");
  Serial.println(start_time);
  while (millis() - start_time < 100){  //100の部分を変更すれば待機時間を変更できる
  }
  digitalWrite(led, LOW);
  analogWrite(active, 0);
  return start_time;
}
//計測ボタン動作関数
bool button_measure(int measure_button, unsigned long &end_time){
  static int stableState_measure = HIGH;
  static int lastReading_measure = HIGH;
  static unsigned long lastChangeTime_measure = 0;
  int end_value = digitalRead(measure_button);
  if (end_value != lastReading_measure){
    lastChangeTime_measure = millis();
    lastReading_measure = end_value;
  }
  if (millis() - lastChangeTime_measure >= DEBOUNCE_MS){
    if (stableState_measure != end_value){
      stableState_measure = end_value;
      if (stableState_measure == LOW){
        end_time = millis();
        Serial.print("end_time = ");
        Serial.println(end_time);
        return true;
      }
    }
  }
  return false;
}
//成功・失敗判定関数
int suc_fail(unsigned long time_start, unsigned long time_end, unsigned long &total_time){
  total_time = time_end - time_start;
  int patern;
  if (total_time >= 10000){
    patern = 2;
  } else if (total_time <= 433){
    patern = 1;
  } else {
    patern = 0;
  }
  switch (patern){
    case 1:
      Serial.println("Success");
      return 1;  // 433ms以下: 成功
    case 2:
      Serial.println("Error");
      return 2;  // 10000ms以上: タイムオーバー
    default:
      Serial.println("Fail");
      return 0;  // 434ms以上9999ms以下: 失敗
  }
}
//パッシブブザー動作関数
void passive_state(int result_number){
  if (result_number == 1){
    delay(2000);
    tone(PIN_BUZZER_PASSIVE, 988, 120);
    delay(150);
    tone(PIN_BUZZER_PASSIVE, 1319, 180);
    delay(220);
    noTone(PIN_BUZZER_PASSIVE);
    return;
  }
  if (result_number == 0 || result_number == 2){
    delay(2000);
    tone(PIN_BUZZER_PASSIVE, 392, 220);
    delay(260);
    tone(PIN_BUZZER_PASSIVE, 262, 350);
    delay(390);
    noTone(PIN_BUZZER_PASSIVE);
  }
}
//4桁の7セグメントディスプレイへの結果表示関数
void seg_47(unsigned long total_time, int after){
  Serial.print("total_time = ");
  Serial.println(total_time);
  int nums[4];
  bool dots[4] = {false, false, false, false};

  if (after == 1 || after == 0){
    nums[0] = (total_time / 1000) % 10; // 千の位
    nums[1] = (total_time / 100) % 10;  // 百の位
    nums[2] = (total_time / 10) % 10;   // 十の位
    nums[3] = total_time % 10;          // 一の位
    dots[0] = true;
    dots[1] = false;  // 2桁目に小数点
    dots[2] = false;
    dots[3] = false;
    if(total_time < 1000) nums[0]=0;
  }
  if (after == 2){
    for(int i=0;i<4;i++) nums[i]=17;
  }
  // ダイナミック点灯
  for (int rep = 0; rep < 100; rep++) { // ループ回数で表示の明るさ調整
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        digitalWrite(digitPins[j], HIGH); // 先に全桁OFF
      }
      Display(nums[i], dots[i]);
      digitalWrite(digitPins[i], LOW); // 対象桁のみON
      delay(2); // 2ms点灯
      digitalWrite(digitPins[i], HIGH);  // 桁OFF
    }
  }
}
//4桁7セグの1桁分の点灯関数
void Display(unsigned char num, bool dot){
  digitalWrite(latch,LOW);
  unsigned char seg = table[num];
  if(dot) seg |= 0x80; // 小数点ON
  shiftOut(data,clock,MSBFIRST,seg);
  digitalWrite(latch,HIGH);
}