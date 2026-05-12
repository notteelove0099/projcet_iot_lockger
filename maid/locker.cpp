// ===================================
// locker.cpp — จัดการ Relay และเซนเซอร์
// ===================================

#include "locker.h"
#include "config.h"

LockerState lockerStates[3];

void lockerSetup() {
  // 1. บังคับให้ขา Relay เป็นการ "จ่ายไฟออก" (ถ้าไม่มีตรงนี้ Relay จะไม่ทำงาน)
  pinMode(RELAY_A_PIN, OUTPUT);
  pinMode(RELAY_B_PIN, OUTPUT);
  pinMode(RELAY_C_PIN, OUTPUT);

  // 2. สั่ง ปิด Relay ทันทีตอนบูตเครื่อง (Active LOW คือการส่ง HIGH เพื่อปิดวงจร)
  digitalWrite(RELAY_A_PIN, HIGH);
  digitalWrite(RELAY_B_PIN, HIGH);
  digitalWrite(RELAY_C_PIN, HIGH);

  // 3. ตั้งค่า Magnetic Sensor
  pinMode(MAG_A_PIN, INPUT);
  pinMode(MAG_B_PIN, INPUT);
  pinMode(MAG_C_PIN, INPUT);

  for (int i = 0; i < 3; i++) {
    lockerStates[i].isOpen = false;
    lockerStates[i].isUnlocking = false;
    lockerStates[i].unlockTimer = 0;
    lockerStates[i].failCount = 0;
    lockerStates[i].lockoutUntil = 0;
  }
}

void lockerUnlock(LockerId id) {
  int pin = (id == LOCKER_A) ? RELAY_A_PIN :
            (id == LOCKER_B) ? RELAY_B_PIN : RELAY_C_PIN;
  
  // สั่ง เปิด Relay (Active LOW คือส่ง LOW เพื่อทริกเกอร์แม่เหล็ก)
  digitalWrite(pin, LOW);
  
  lockerStates[id].isUnlocking = true;
  lockerStates[id].unlockTimer = millis();
}

void lockerLoop() {
  unsigned long now = millis();

  // จัดการการ ปิด รีเลย์อัตโนมัติเมื่อครบ 2 วินาที
  for (int i = 0; i < 3; i++) {
    if (lockerStates[i].isUnlocking) {
      if (now - lockerStates[i].unlockTimer >= UNLOCK_PULSE_MS) {
        int pin = (i == LOCKER_A) ? RELAY_A_PIN :
                  (i == LOCKER_B) ? RELAY_B_PIN : RELAY_C_PIN;
                  
        // หมดเวลาปลดล็อค สั่ง ปิด รีเลย์กลับที่เดิม
        digitalWrite(pin, HIGH);
        lockerStates[i].isUnlocking = false;
      }
    }
  }

  // อ่านค่า Magnetic Sensor
  lockerStates[LOCKER_A].isOpen = (digitalRead(MAG_A_PIN) != MAG_CLOSED);
  lockerStates[LOCKER_B].isOpen = (digitalRead(MAG_B_PIN) != MAG_CLOSED);
  lockerStates[LOCKER_C].isOpen = (digitalRead(MAG_C_PIN) != MAG_CLOSED);
}

bool lockerIsLockedOut(LockerId id) {
  if (lockerStates[id].failCount >= MAX_AUTH_FAIL) {
    if (millis() < lockerStates[id].lockoutUntil) return true;
    lockerResetFail(id); // พ้นระยะเวลาล็อคแล้ว
  }
  return false;
}

void lockerRecordFail(LockerId id) {
  lockerStates[id].failCount++;
  if (lockerStates[id].failCount >= MAX_AUTH_FAIL) {
    lockerStates[id].lockoutUntil = millis() + LOCKOUT_MS;
  }
}

void lockerResetFail(LockerId id) {
  lockerStates[id].failCount = 0;
  lockerStates[id].lockoutUntil = 0;
}