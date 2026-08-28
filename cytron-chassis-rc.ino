#include "CytronMotorDriver.h"

// RC vstupy (interrupt piny) - Arduino Nano
// Tank mode: CH1 = levý motor, CH2 = pravý motor
const byte CH1_PIN = 2;   // levý motor
const byte CH2_PIN = 3;   // pravý motor
const byte TEST_BTN = 4;  // tlačítko test sekvence (INPUT_PULLUP)
const unsigned long FAILSAFE_TIMEOUT_US = 50000;  // 50 ms bez pulzu = ztráta signálu

// Napěťové omezení motoru
// 12V motor na 4S baterii (max 16.8 V) -> omezit PWM, aby motor nedostal > 12V
const float MOTOR_VOLTAGE = 12.0;       // jmenovité napětí motoru [V]
const float BATTERY_VOLTAGE_MAX = 16.8; // maximální napětí baterie (4S plně nabitá) [V]
const int MAX_PWM = constrain((int)(255.0 * MOTOR_VOLTAGE / BATTERY_VOLTAGE_MAX), 0, 255); // max PWM odpovídající 12V, omezeno na platný rozsah

volatile unsigned long ch1_start = 0;
volatile int ch1_value = 1500;
volatile unsigned long ch2_start = 0;
volatile int ch2_value = 1500;
volatile unsigned long ch1_last = 0;  // poslední čas pulzu CH1
volatile unsigned long ch2_last = 0;  // poslední čas pulzu CH2

CytronMD motorL(PWM_DIR, 5, 6);   // levý motor
CytronMD motorR(PWM_DIR, 9, 10);  // pravý motor

void ch1_isr() {
  if (digitalRead(CH1_PIN) == HIGH) {
    ch1_start = micros();
  } else {
    ch1_value = micros() - ch1_start;
    ch1_last = micros();
  }
}

void ch2_isr() {
  if (digitalRead(CH2_PIN) == HIGH) {
    ch2_start = micros();
  } else {
    ch2_value = micros() - ch2_start;
    ch2_last = micros();
  }
}

void runTestSequence() {
  motorL.setSpeed(0);
  motorR.setSpeed(0);
  delay(100);

  motorL.setSpeed(MAX_PWM);   // 2 s motor 1 (L) vpřed (omezeno na 12V)
  motorR.setSpeed(0);
  delay(2000);

  motorL.setSpeed(0);         // 2 s motor 2 (R) vpřed
  motorR.setSpeed(MAX_PWM);
  delay(2000);

  motorL.setSpeed(MAX_PWM);   // 1 s oba vpřed
  motorR.setSpeed(MAX_PWM);
  delay(1000);

  motorL.setSpeed(-MAX_PWM);  // 1 s oba vzad
  motorR.setSpeed(-MAX_PWM);
  delay(1000);

  motorL.setSpeed(0);         // zastavit
  motorR.setSpeed(0);
}

void setup() {
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(TEST_BTN, INPUT_PULLUP);
  ch1_last = micros();
  ch2_last = micros();
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), ch1_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2_isr, CHANGE);
}

void loop() {
  // Test sekvence po stisku tlačítka
  if (digitalRead(TEST_BTN) == LOW) {
    delay(50);                     // debounce
    if (digitalRead(TEST_BTN) == LOW) {
      runTestSequence();
      while (digitalRead(TEST_BTN) == LOW) { delay(10); }  // čekat na uvolnění tlačítka
    }
    return;
  }

  // Atomické čtení na 8bit AVR (Mega), aby ISR nepřerušilo zápis/čtení 16bit int
  noInterrupts();
  int leftInput  = ch1_value;
  int rightInput = ch2_value;
  unsigned long t1 = ch1_last;
  unsigned long t2 = ch2_last;
  unsigned long now = micros();
  interrupts();

  // Failsafe – signál mimo rozsah nebo žádný nový pulz > 50 ms
  if (leftInput < 900 || leftInput > 2100 ||
      rightInput < 900 || rightInput > 2100 ||
      (now - t1) > FAILSAFE_TIMEOUT_US ||
      (now - t2) > FAILSAFE_TIMEOUT_US) {
    motorL.setSpeed(0);
    motorR.setSpeed(0);
    return;
  }

  leftInput  = constrain(leftInput, 1000, 2000);
  rightInput = constrain(rightInput, 1000, 2000);

  // Tank mode: CH1 přímo ovládá levý motor, CH2 pravý motor
  int leftSpeed  = map(leftInput,  1000, 2000, -MAX_PWM, MAX_PWM);
  int rightSpeed = map(rightInput, 1000, 2000, -MAX_PWM, MAX_PWM);

  motorL.setSpeed(leftSpeed);
  motorR.setSpeed(rightSpeed);
}
