// ===================================
// buzzer.cpp — ควบคุม Buzzer
// ===================================

#include "buzzer.h"
#include "config.h"

void buzzerSetup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

// เสียงสำเร็จ: ดัง 1 ครั้ง 200ms
void buzzerSuccess() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}

// เสียงผิดพลาด: ดัง 2 ครั้ง สั้น
void buzzerFail() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

// เสียงแจ้งเตือน: ดัง 3 ครั้ง ยาว
void buzzerAlert() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(250);
  }
}

// หยุด Buzzer ทันที
void buzzerStop() {
  digitalWrite(BUZZER_PIN, LOW);
}
