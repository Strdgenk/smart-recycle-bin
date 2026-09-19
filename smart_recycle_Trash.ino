#include <Servo.h>
#include <avr/interrupt.h>

/************************************************
 **************** 핀 설정 ************************
 ************************************************/

/********* 쓰레기 감지 IR *********/
#define Trash_IR_IN 12

/********* X축 위치 감지 IR *********/
#define X_MOVE_IR_PIN A2

/********* 초음파 센서 *********/
#define TRASH_CHO_1_TRIG 3
#define TRASH_CHO_1_ECHO 7

#define TRASH_CHO_2_TRIG 2
#define TRASH_CHO_2_ECHO 6

#define TRASH_CHO_3_TRIG 9
#define TRASH_CHO_3_ECHO 5

#define TRASH_CHO_4_TRIG 8
#define TRASH_CHO_4_ECHO 4

/********* 시프트레지스터 *********/
#define MOTOR_SHIFT_DATA SCL
#define MOTOR_SHIFT_CLK  SDA
#define MOTOR_SHIFT_STR  13

/********* 액추에이터 비트 *********/
#define MOTOR_ACT1_BIT 0
#define MOTOR_ACT2_BIT 1

/********* 바퀴 모터 *********/
/*
  RIGHT = 1010
  LEFT  = 0101
*/

const int WHEEL_IN1 = 10;
const int WHEEL_IN2 = 11;
const int WHEEL_IN3 = 44;
const int WHEEL_IN4 = 45;

/********* 소프트 속도 제어 *********/
const int MOTOR_ON_DELAY  = 6;
const int MOTOR_OFF_DELAY = 4;

/********* LED *********/
#define LED_GENERAL 22
#define LED_PLASTIC 23
#define LED_GLASS   24
#define LED_CAN     25

/********* 테스트 버튼 *********/
#define TEST_BUTTON 26

/********* 금속 센서 *********/
#define IRON_SEN1 8

/********* 압력 센서 *********/
#define PUSH_SEN_1 A0
#define PUSH_SEN_2 A1

/********* 서보모터 *********/
#define SERVO_MO_IN1 14
#define SERVO_MO_IN2 15
#define SERVO_MO_IN3 16

/************************************************
 **************** 서보모터 ************************
 ************************************************/

Servo gateServo;
Servo trashServo1;
Servo trashServo2;

/************************************************
 **************** 전역 변수 **********************
 ************************************************/

byte registerState = 0;

String inputString = "";

int trash_ir_val;
int iron_val;
int lastB = HIGH;

/********* 초음파 변수 *********/
long duration_1, duration_2, duration_3, duration_4;
long distance_1, distance_2, distance_3, distance_4;

int per_1, per_2, per_3, per_4;

/********* 적재량 기준 *********/
const int BIN_EMPTY = 30;
const int BIN_FULL  = 5;

/********* 타이머 변수 *********/
volatile int timerCount = 0;
volatile bool printFlag = false;

/************************************************
 **************** Timer2 인터럽트 ****************
 ************************************************/

ISR(TIMER2_OVF_vect) {

  TCNT2 = 6;

  timerCount++;

  if (timerCount >= 500) {

    timerCount = 0;
    printFlag = true;
  }
}

/************************************************
 **************** LED 제어 ***********************
 ************************************************/

void allOff() {

  digitalWrite(LED_GENERAL, LOW);
  digitalWrite(LED_PLASTIC, LOW);
  digitalWrite(LED_GLASS, LOW);
  digitalWrite(LED_CAN, LOW);
}

/************************************************
 **************** 센서 함수 **********************
 ************************************************/

void readTrashIR() {

  trash_ir_val = digitalRead(Trash_IR_IN);
}

void readIronSen() {

  iron_val = digitalRead(IRON_SEN1);
}

/************************************************
 **************** 초음파 함수 ********************
 ************************************************/

void CHO_SEN_1() {

  digitalWrite(TRASH_CHO_1_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRASH_CHO_1_TRIG, LOW);

  duration_1 = pulseIn(TRASH_CHO_1_ECHO, HIGH);
  distance_1 = duration_1 / 29 / 2;
}

void CHO_SEN_2() {

  digitalWrite(TRASH_CHO_2_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRASH_CHO_2_TRIG, LOW);

  duration_2 = pulseIn(TRASH_CHO_2_ECHO, HIGH);
  distance_2 = duration_2 / 29 / 2;
}

void CHO_SEN_3() {

  digitalWrite(TRASH_CHO_3_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRASH_CHO_3_TRIG, LOW);

  duration_3 = pulseIn(TRASH_CHO_3_ECHO, HIGH);
  distance_3 = duration_3 / 29 / 2;
}

void CHO_SEN_4() {

  digitalWrite(TRASH_CHO_4_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRASH_CHO_4_TRIG, LOW);

  duration_4 = pulseIn(TRASH_CHO_4_ECHO, HIGH);
  distance_4 = duration_4 / 29 / 2;
}

/************************************************
 ************** 적재량 계산 **********************
 ************************************************/

void calculatePercentage() {

  per_1 = constrain(map(distance_1, BIN_EMPTY, BIN_FULL, 0, 100), 0, 100);
  per_2 = constrain(map(distance_2, BIN_EMPTY, BIN_FULL, 0, 100), 0, 100);
  per_3 = constrain(map(distance_3, BIN_EMPTY, BIN_FULL, 0, 100), 0, 100);
  per_4 = constrain(map(distance_4, BIN_EMPTY, BIN_FULL, 0, 100), 0, 100);
}

/************************************************
 **************** 서보 함수 **********************
 ************************************************/

void gateClose() {

  gateServo.write(0);
}

void gateOpen() {

  gateServo.write(90);
}

void trashDoorOpen() {

  trashServo1.write(90);
  trashServo2.write(90);
}

void trashDoorClose() {

  trashServo1.write(0);
  trashServo2.write(0);
}

/************************************************
 ************** 액추에이터 함수 *****************
 ************************************************/

void updateShiftRegister() {

  digitalWrite(MOTOR_SHIFT_STR, LOW);

  shiftOut(MOTOR_SHIFT_DATA,
           MOTOR_SHIFT_CLK,
           MSBFIRST,
           registerState);

  digitalWrite(MOTOR_SHIFT_STR, HIGH);
}

void actuatorForward() {

  bitSet(registerState, MOTOR_ACT1_BIT);
  bitClear(registerState, MOTOR_ACT2_BIT);

  updateShiftRegister();
}

void actuatorBackward() {

  bitClear(registerState, MOTOR_ACT1_BIT);
  bitSet(registerState, MOTOR_ACT2_BIT);

  updateShiftRegister();
}

void actuatorStop() {

  bitClear(registerState, MOTOR_ACT1_BIT);
  bitClear(registerState, MOTOR_ACT2_BIT);

  updateShiftRegister();
}

/************************************************
 **************** 바퀴 모터 함수 *****************
 *
 * 소프트 속도 제어
 ************************************************/

void wheelMoveRight() {

  digitalWrite(WHEEL_IN1, HIGH);
  digitalWrite(WHEEL_IN2, LOW);

  digitalWrite(WHEEL_IN3, HIGH);
  digitalWrite(WHEEL_IN4, LOW);

  delay(MOTOR_ON_DELAY);

  digitalWrite(WHEEL_IN1, LOW);
  digitalWrite(WHEEL_IN2, LOW);

  digitalWrite(WHEEL_IN3, LOW);
  digitalWrite(WHEEL_IN4, LOW);

  delay(MOTOR_OFF_DELAY);
}

void wheelMoveLeft() {

  digitalWrite(WHEEL_IN1, LOW);
  digitalWrite(WHEEL_IN2, HIGH);

  digitalWrite(WHEEL_IN3, LOW);
  digitalWrite(WHEEL_IN4, HIGH);

  delay(MOTOR_ON_DELAY);

  digitalWrite(WHEEL_IN1, LOW);
  digitalWrite(WHEEL_IN2, LOW);

  digitalWrite(WHEEL_IN3, LOW);
  digitalWrite(WHEEL_IN4, LOW);

  delay(MOTOR_OFF_DELAY);
}

void wheelStop() {

  digitalWrite(WHEEL_IN1, LOW);
  digitalWrite(WHEEL_IN2, LOW);

  digitalWrite(WHEEL_IN3, LOW);
  digitalWrite(WHEEL_IN4, LOW);
}

/************************************************
 ************** X축 이동 함수 ********************
 ************************************************/

int moveToPosition(int targetCount) {

  int movedCount = 0;
  int lastIR = HIGH;

  while (movedCount < targetCount) {

    wheelMoveRight();

    int val = digitalRead(X_MOVE_IR_PIN);

    if (val == LOW && lastIR == HIGH) {

      lastIR = LOW;

      movedCount++;

      Serial.print("X COUNT FORWARD : ");
      Serial.println(movedCount);

      delay(100);
    }
    else if (val == HIGH && lastIR == LOW) {

      lastIR = HIGH;
    }
  }

  wheelStop();

  return movedCount;
}

/************************************************
 ************** X축 복귀 함수 ********************
 ************************************************/

void returnToGeneral(int targetCount) {

  int returnCount = 0;
  int lastIR = HIGH;

  while (returnCount < targetCount) {

    wheelMoveLeft();

    int val = digitalRead(X_MOVE_IR_PIN);

    if (val == LOW && lastIR == HIGH) {

      lastIR = LOW;

      returnCount++;

      Serial.print("X COUNT RETURN : ");
      Serial.println(returnCount);

      delay(100);
    }
    else if (val == HIGH && lastIR == LOW) {

      lastIR = HIGH;
    }
  }

  wheelStop();
}

/************************************************
 ************** 액추에이터 동작 ******************
 ************************************************/

void runActuator() {

  actuatorForward();
  delay(3000);

  actuatorStop();
  delay(500);

  actuatorBackward();
  delay(3000);

  actuatorStop();
}

/************************************************
 ************** 분류 메인 함수 *******************
 ************************************************/

void processTrash(String type) {

  allOff();

  trashDoorOpen();

  delay(1500);

  trashDoorClose();

  int movedCount = 0;

  if (type == "GENERAL") {

    digitalWrite(LED_GENERAL, HIGH);
    movedCount = 0;
  }
  else if (type == "PLASTIC") {

    digitalWrite(LED_PLASTIC, HIGH);
    movedCount = moveToPosition(2);
  }
  else if (type == "CAN") {

    digitalWrite(LED_CAN, HIGH);
    movedCount = moveToPosition(3);
  }
  else if (type == "GLASS") {

    digitalWrite(LED_GLASS, HIGH);
    movedCount = moveToPosition(4);
  }

  runActuator();

  if (movedCount > 0) {

    returnToGeneral(movedCount);
  }

  CHO_SEN_1(); delay(30);
  CHO_SEN_2(); delay(30);
  CHO_SEN_3(); delay(30);
  CHO_SEN_4(); delay(30);

  calculatePercentage();

  Serial.print("GENERAL : ");
  Serial.print(per_1);
  Serial.print("%, ");

  Serial.print("PLASTIC : ");
  Serial.print(per_2);
  Serial.print("%, ");

  Serial.print("CAN : ");
  Serial.print(per_3);
  Serial.print("%, ");

  Serial.print("GLASS : ");
  Serial.print(per_4);
  Serial.println("%");

  allOff();

  gateOpen();
}

/************************************************
 **************** 시리얼 처리 ********************
 ************************************************/

void serialEventProcess() {

  if (Serial.available()) {

    inputString = Serial.readStringUntil('\n');

    inputString.trim();

    Serial.print("RECEIVED : ");
    Serial.println(inputString);

    if (inputString == "GENERAL" ||
        inputString == "PLASTIC" ||
        inputString == "CAN" ||
        inputString == "GLASS") {

      processTrash(inputString);
    }
    else {

      Serial.println("UNKNOWN TYPE");
    }
  }
}

/************************************************
 ******************** SETUP *********************
 ************************************************/

void setup() {

  Serial.begin(115200);

  /********* 센서 *********/
  pinMode(Trash_IR_IN, INPUT_PULLUP);
  pinMode(X_MOVE_IR_PIN, INPUT_PULLUP);

  pinMode(IRON_SEN1, INPUT);

  pinMode(TEST_BUTTON, INPUT_PULLUP);

  pinMode(PUSH_SEN_1, INPUT);
  pinMode(PUSH_SEN_2, INPUT);

  /********* 초음파 *********/
  pinMode(TRASH_CHO_1_TRIG, OUTPUT);
  pinMode(TRASH_CHO_1_ECHO, INPUT);

  pinMode(TRASH_CHO_2_TRIG, OUTPUT);
  pinMode(TRASH_CHO_2_ECHO, INPUT);

  pinMode(TRASH_CHO_3_TRIG, OUTPUT);
  pinMode(TRASH_CHO_3_ECHO, INPUT);

  pinMode(TRASH_CHO_4_TRIG, OUTPUT);
  pinMode(TRASH_CHO_4_ECHO, INPUT);

  /********* 시프트레지스터 *********/
  pinMode(MOTOR_SHIFT_DATA, OUTPUT);
  pinMode(MOTOR_SHIFT_CLK, OUTPUT);
  pinMode(MOTOR_SHIFT_STR, OUTPUT);

  /********* 바퀴 모터 *********/
  pinMode(WHEEL_IN1, OUTPUT);
  pinMode(WHEEL_IN2, OUTPUT);
  pinMode(WHEEL_IN3, OUTPUT);
  pinMode(WHEEL_IN4, OUTPUT);

  /********* LED *********/
  pinMode(LED_GENERAL, OUTPUT);
  pinMode(LED_PLASTIC, OUTPUT);
  pinMode(LED_GLASS, OUTPUT);
  pinMode(LED_CAN, OUTPUT);

  /********* 서보 *********/
  gateServo.attach(SERVO_MO_IN1);
  trashServo1.attach(SERVO_MO_IN2);
  trashServo2.attach(SERVO_MO_IN3);

  /********* 초기 상태 *********/
  allOff();

  gateOpen();

  trashDoorClose();

  actuatorStop();

  registerState = 0;

  updateShiftRegister();

  /********* Timer2 *********/
  TCCR2A = 0x00;
  TCCR2B = 0x05;

  TCNT2 = 6;

  TIMSK2 = 0x01;

  sei();

  Serial.println("SYSTEM READY");
}

/************************************************
 ********************* LOOP *********************
 ************************************************/

void loop() {

  readTrashIR();

  readIronSen();

  /********* 테스트 버튼 *********/
  bool currentButton = digitalRead(TEST_BUTTON);

  if (currentButton == LOW && lastB == HIGH) {

    Serial.println("TRASH_DETECTED");

    delay(50);
  }

  lastB = currentButton;

  /********* 쓰레기 감지 *********/
  if (trash_ir_val == LOW) {

    gateClose();

    delay(500);

    Serial.println("TRASH_DETECTED");

    while (digitalRead(Trash_IR_IN) == LOW);
  }

  /********* 시리얼 처리 *********/
  serialEventProcess();

  /********* 1초 출력 *********/
  if (printFlag == true) {

    printFlag = false;

    Serial.println("SYSTEM RUNNING");
  }
}
