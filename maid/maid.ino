// ===================================
// main.cpp — โค้ดหลัก ESP32
// ===================================

#include <Arduino.h>
#include "config.h"
#include "buzzer.h"
#include "lcd.h"
#include "my_keypad.h"
#include "fingerprint.h"
#include "locker.h"
#include "api_client.h"

// ── Forward Declarations ───────────────────────────────────────────────────
void checkEnrollment();
void handleKeyPress(char key);
void handleFingerprintAuth(int fpId);
void handlePinAuth(char lockerId, const char* pin);
void doUnlock(LockerId id, const char* userId, const char* method);

// ── State ของระบบ ────────────────────────────────────────────────────────
enum SystemMode {
  MODE_IDLE,        
  MODE_ENTER_PIN,   
  MODE_ENROLLING    
};

SystemMode mode = MODE_IDLE;
SystemMode lastMode = (SystemMode)-1; 

char    pinBuffer[5]    = "";
int     pinLength       = 0;
char    pinTargetLocker = 0;

unsigned long lastHeartbeat = 0;
unsigned long lastEnrollCheck = 0;
unsigned long lastRemoteUnlockCheck = 0; 

#define ENROLL_CHECK_INTERVAL_MS 10000
#define REMOTE_UNLOCK_INTERVAL_MS 2000   

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Locker ESP32 Boot ===");

  buzzerSetup();
  lcdSetup();
  keypadSetup();
  fingerprintSetup();
  lockerSetup();
  apiSetup();

  if (apiIsOnline()) {
    lcdShow("Smart Locker", "Online Ready");
    apiPostEvent('A', "SYSTEM_BOOT", 0, "", 0, "ESP32 restarted");
  } else {
    lcdOffline();
  }

  delay(1500);
  lcdReady();

  Serial.println("=== ระบบพร้อมใช้งาน ===");
}

// ── Loop ─────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (mode != lastMode) {
    lastMode = mode;
    Serial.print("🔄 สถานะระบบปัจจุบัน: ");
    switch (mode) {
      case MODE_IDLE:      
        Serial.println("MODE_IDLE (พร้อมใช้งาน: รอสแกนนิ้ว หรือ กด 1,2,3 เพื่อเลือกตู้ A,B,C สำหรับใส่ PIN)"); 
        break;
      case MODE_ENTER_PIN: 
        Serial.println("MODE_ENTER_PIN (กำลังรอผู้ใช้งานกดรหัส PIN 4 หลัก)"); 
        break;
      case MODE_ENROLLING: 
        Serial.println("MODE_ENROLLING (กำลังลงทะเบียนลายนิ้วมือใหม่ให้ User)"); 
        break;
    }
  }

  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = now;
    apiSendHeartbeat();
    if (!apiIsOnline()) lcdOffline();
  }

  if (mode == MODE_IDLE && (now - lastRemoteUnlockCheck >= REMOTE_UNLOCK_INTERVAL_MS)) {
    lastRemoteUnlockCheck = now;
    char targetLocker;
    if (apiCheckRemoteUnlock(targetLocker)) {
      LockerId id = (LockerId)(targetLocker - 'A');
      Serial.printf("🔓 ได้รับคำสั่ง Remote Unlock ช่อง %c จาก Admin\n", targetLocker);
      doUnlock(id, "", "REMOTE_UNLOCK");
    }
  }

  if (mode == MODE_IDLE && (now - lastEnrollCheck >= ENROLL_CHECK_INTERVAL_MS)) {
    lastEnrollCheck = now;
    checkEnrollment();
  }

  lockerLoop();

  char key = keypadGetKey();
  if (key) handleKeyPress(key);

  if (mode == MODE_IDLE) {
    int fpId = fingerprintScan();
    
    if (fpId > 0) {
      handleFingerprintAuth(fpId); 
    } 
    else if (fpId == -2) {
      Serial.println("❌ สแกนนิ้วแล้ว แต่ไม่พบลายนิ้วมือนี้ในระบบ");
      lcdError("Wrong Finger");
      buzzerFail();
      delay(1500);
      lcdReady();
    }
    else if (fpId == -3) {
      // Error จากเซนเซอร์
    }
  }

  delay(50);
}

// ── ตรวจ Enrollment Pending (หน่วงเวลา 30 วิแรก เช็คเว็บก่อนบังคับกด) ──
void checkEnrollment() {
  if (!apiIsOnline()) return;

  char   pendingUserId[32] = "";
  char   pendingName[64]   = "";
  uint8_t slot             = 0;

  bool hasPending = apiCheckPendingEnroll(pendingUserId, pendingName, slot);
  if (!hasPending) return; 

  Serial.printf("📋 มี User ใหม่รอลงทะเบียนนิ้ว: %s (slot %d)\n", pendingName, slot);
  mode = MODE_ENROLLING;

  lcdShow("Enroll:", pendingName);
  delay(1500);
  
  // 🟢 เริ่มต้น: ยังไม่บังคับกด แต่โชว์ว่ากำลังให้เวลาเว็บจัดการ 30 วินาที
  lcdShow("Checking Web...", "Please wait 30s");
  Serial.println("รอตรวจสอบสถานะจากหน้าเว็บ 30 วินาทีแรก...");

  bool isConfirmed = false;
  bool promptShown = false; 
  unsigned long waitStart = millis();
  unsigned long lastApiCheck = millis(); 
  
  // 🟢 ขยายเวลาลูปรวมเป็น 60 วินาที (รอเว็บ 30 วิ + ให้เวลากดปุ่มหน้าตู้อีก 30 วิ)
  while (millis() - waitStart < 60000) { 
    
    // 🟢 ถ้าผ่านไป 30 วินาทีแล้ว ยังไม่กดยกเลิกในเว็บ ค่อยบังคับให้กด # หรือ *
    if (!promptShown && (millis() - waitStart >= 30000)) {
        promptShown = true;
        lcdShow("Press # to scan", "Press * to skip");
        Serial.println("หมดเวลา 30 วิ ไม่พบการยกเลิกจากเว็บ -> รอให้ผู้ใช้กดปุ่มหน้าตู้...");
    }

    char k = keypadGetKey();
    
    // 🟢 อนุญาตให้กด # หรือ * ได้เฉพาะตอนที่โชว์หน้าจอให้กดแล้วเท่านั้น (ผ่าน 30 วิไปแล้ว)
    if (promptShown && k == '#') {
      lcdShow("Checking...", "Please wait");
      char tId[32]="", tName[64]="";
      uint8_t tSlot=0;
      bool stillPending = apiCheckPendingEnroll(tId, tName, tSlot);
      
      if (!stillPending || strcmp(tId, pendingUserId) != 0) {
         Serial.println("🌐 คิวถูกยกเลิกไปแล้ว! ตัดจบการเปิดเซนเซอร์");
         lcdShow("Canceled", "from Web"); 
         buzzerSuccess(); 
         delay(1500);
         mode = MODE_IDLE;
         lcdReady();
         return; 
      }
      isConfirmed = true;   
      break;

    } else if (promptShown && k == '*') {
      break; 
    }

    // 🟢 ตรวจสอบกับ Server อัตโนมัติทุก 1.5 วินาทีตลอดช่วง 60 วิ
    if (millis() - lastApiCheck >= 1500) {
      lastApiCheck = millis();
      char tId[32]="", tName[64]="";
      uint8_t tSlot=0;
      
      bool stillPending = apiCheckPendingEnroll(tId, tName, tSlot);
      
      if (!stillPending || strcmp(tId, pendingUserId) != 0) {
         Serial.println("🌐 ตรวจพบการกดยกเลิกจากหน้าเว็บ ตัดจบการรอหน้าตู้ทันที!");
         lcdShow("Canceled", "from Web"); 
         buzzerSuccess(); 
         delay(1500);
         mode = MODE_IDLE;
         lcdReady();
         return; 
      }
    }

    delay(50); 
  }

  // ถ้าหมดเวลาลูป (60 วิ) หรือกด * ที่ตู้เอง
  if (!isConfirmed) {
    Serial.println("⏩ ข้ามการลงทะเบียนที่หน้าตู้ (สั่งเคลียร์สถานะใน Database)");
    
    lcdShow("Enroll Later", "Skip for now"); 
    buzzerSuccess(); 
    apiCancelEnroll(pendingUserId);
    
    delay(2000);
    mode = MODE_IDLE;
    lcdReady();
    return; 
  }

  lcdShow("Place finger", "on sensor");
  buzzerSuccess(); 

  bool success = fingerprintEnroll(slot);

  if (success) {
    apiEnrollComplete(pendingUserId, slot);
    lcdShow("Enrolled!", pendingName);
    buzzerSuccess();
    Serial.printf("✅ ลงทะเบียนนิ้ว %s slot %d สำเร็จ\n", pendingName, slot);
  } else {
    lcdError("Enroll fail");
    buzzerFail();
    Serial.println("❌ ลงทะเบียนนิ้วไม่สำเร็จ");
  }

  delay(2000);
  mode = MODE_IDLE;
  lcdReady();
}

// ── จัดการการกด Key ────────────────────────────────────────────────────────
void handleKeyPress(char key) {
  Serial.printf("กด: %c\n", key);

  if (mode == MODE_IDLE) {
    if (key == '1' || key == '2' || key == '3') {
      if (key == '1') pinTargetLocker = 'A';
      else if (key == '2') pinTargetLocker = 'B';
      else if (key == '3') pinTargetLocker = 'C';

      mode            = MODE_ENTER_PIN;
      pinLength       = 0;
      memset(pinBuffer, 0, sizeof(pinBuffer));
      lcdEnterPin(pinTargetLocker);
    }
    else if (key == '*') {
      lcdReady();
    }
  }
  else if (mode == MODE_ENTER_PIN) {
    if (key == '#') {
      if (pinLength == 4) {
        handlePinAuth(pinTargetLocker, pinBuffer);
      } else {
        lcdError("PIN 4 หลัก");
        delay(1500);
        lcdEnterPin(pinTargetLocker);
      }
    }
    else if (key == '*') {
      mode      = MODE_IDLE;
      pinLength = 0;
      memset(pinBuffer, 0, sizeof(pinBuffer));
      lcdReady();
    }
    else if (key >= '0' && key <= '9' && pinLength < 4) {
      pinBuffer[pinLength++] = key;
      pinBuffer[pinLength]   = '\0';

      char stars[5] = "    ";
      for (int i = 0; i < pinLength; i++) stars[i] = '*';
      stars[pinLength] = '\0';
      lcdShow("Enter PIN:", stars);
    }
  }
}

// ── ยืนยันด้วย Fingerprint ────────────────────────────────────────────────
void handleFingerprintAuth(int fpId) {
  lcdScanning();

  char userId[32]    = "";
  char lockers[16]   = "";

  if (!apiIsOnline()) {
    lcdError("Offline");
    buzzerFail();
    delay(1500);
    lcdOffline();
    return;
  }

  bool found = apiVerifyFingerprint(fpId, userId, lockers);

  if (!found) {
    Serial.println("ไม่พบ User จาก fingerprint");
    lcdError("Not found");
    buzzerFail();
    delay(1500);
    lcdReady();
    return;
  }

  bool opened = false;
  const char* p = lockers;

  while (*p) {
    if (*p == 'A' || *p == 'B' || *p == 'C') {
      LockerId id = (LockerId)(*p - 'A');
      if (!lockerIsLockedOut(id)) {
        doUnlock(id, userId, "FINGERPRINT");
        opened = true;
      }
    }
    p++;
  }

  if (!opened) {
    lcdError("No access");
    buzzerFail();
    delay(1500);
    lcdReady();
  }
}

// ── ยืนยันด้วย PIN ──────────────────────────────────────────────────────────
void handlePinAuth(char lockerId, const char* pin) {
  lcdShow("Checking PIN", "Please wait...");
  mode = MODE_IDLE;

  LockerId id = (LockerId)(lockerId - 'A');

  if (lockerIsLockedOut(id)) {
    lcdLockout();
    buzzerFail();
    delay(2000);
    lcdReady();
    return;
  }

  char userId[32] = "";

  bool ok = false;
  if (apiIsOnline()) {
    ok = apiVerifyPin(lockerId, pin, userId);
  } else {
    lcdError("Offline");
    buzzerFail();
    delay(1500);
    lcdOffline();
    return;
  }

  if (ok) {
    lockerResetFail(id);
    doUnlock(id, userId, "PIN");
  } else {
    lockerRecordFail(id);
    lcdError("Wrong PIN");
    buzzerFail();
    apiPostEvent(lockerId, "AUTH_FAIL", 0, userId, 0, "Wrong PIN");
    delay(1500);
    lcdReady();
  }

  pinLength = 0;
  memset(pinBuffer, 0, sizeof(pinBuffer));
}

// ── Unlock ตู้ (ทำทุกขั้นตอน) ────────────────────────────────────────────
void doUnlock(LockerId id, const char* userId, const char* method) {
  char lockerId = 'A' + id;
  Serial.printf("✅ Unlock ช่อง %c โดย %s\n", lockerId, method);

  lockerUnlock(id);
  buzzerSuccess();
  lcdUnlocked(lockerId);

  char eventId[32] = "";
  apiPostEvent(lockerId, "UNLOCK_SUCCESS", 0, userId, 0, method);

  apiTriggerCamera(lockerId, eventId);
}