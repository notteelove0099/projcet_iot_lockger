// ===================================
// locker.cpp — จัดการ Relay และเซนเซอร์
// ===================================

#include "locker.h"
#include "config.h"
#include "api_client.h"  // 🟢 ต้อง include ไฟล์นี้เพื่อใช้คำสั่งส่ง Event และถ่ายรูป

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

  // 🟢 4. ตั้งค่าลำโพง Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // ปิดเสียงไว้ก่อนตอนเปิดเครื่อง

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

  // อ่านค่า Magnetic Sensor ปัจจุบัน (ดึงทีเดียวทั้ง 3 ตู้)
  bool currentOpenA = (digitalRead(MAG_A_PIN) != MAG_CLOSED);
  bool currentOpenB = (digitalRead(MAG_B_PIN) != MAG_CLOSED);
  bool currentOpenC = (digitalRead(MAG_C_PIN) != MAG_CLOSED);

  // 🟢 ตรวจจับตู้ A
  if (currentOpenA == true && lockerStates[LOCKER_A].isOpen == false) {
    if (lockerStates[LOCKER_A].isUnlocking == false) {
      Serial.println("🚨 ตู้ A โดนงัด!!");
      digitalWrite(BUZZER_PIN, HIGH); // ร้องเตือน!
      apiPostEvent('A', "ALERT_FORCE_OPEN", 0, "", 0, "ประตูถูกงัด!");
      apiTriggerCamera('A', "FORCE_OPEN");
    }
  }

  // 🟢 ตรวจจับตู้ B
  if (currentOpenB == true && lockerStates[LOCKER_B].isOpen == false) {
    if (lockerStates[LOCKER_B].isUnlocking == false) {
      Serial.println("🚨 ตู้ B โดนงัด!!");
      digitalWrite(BUZZER_PIN, HIGH); // ร้องเตือน!
      apiPostEvent('B', "ALERT_FORCE_OPEN", 0, "", 0, "ประตูถูกงัด!");
      apiTriggerCamera('B', "FORCE_OPEN");
    }
  }

  // 🟢 ตรวจจับตู้ C
  if (currentOpenC == true && lockerStates[LOCKER_C].isOpen == false) {
    if (lockerStates[LOCKER_C].isUnlocking == false) {
      Serial.println("🚨 ตู้ C โดนงัด!!");
      digitalWrite(BUZZER_PIN, HIGH); // ร้องเตือน!
      apiPostEvent('C', "ALERT_FORCE_OPEN", 0, "", 0, "ประตูถูกงัด!");
      apiTriggerCamera('C', "FORCE_OPEN");
    }
  }

  // 🟢 ปิดเสียงร้อง ถ้าโจรตกใจแล้วปิดประตูกลับเหมือนเดิม (หรือเราเดินมาปิด)
  if (!currentOpenA && !currentOpenB && !currentOpenC) {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // อัปเดตสถานะล่าสุดเก็บไว้รอบถัดไป
  lockerStates[LOCKER_A].isOpen = currentOpenA;
  lockerStates[LOCKER_B].isOpen = currentOpenB;
  lockerStates[LOCKER_C].isOpen = currentOpenC;
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