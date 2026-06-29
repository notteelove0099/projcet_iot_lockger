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

// 🟢 ตัวแปรล็อคระบบ (แบบ Global นับรวมทุกตู้เพื่อความชัวร์ 100%)
int globalPinFailCount = 0;
unsigned long globalPinLockoutTime = 0;

int fpFailCount = 0;
unsigned long fpLockoutTime = 0;

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

  // 🟢 จุดดักฟัง Keypad: กดปุ๊บ ปริ้นท์บอกปั๊บ 🟢
  char key = keypadGetKey();
  if (key) {
    Serial.printf("\n[KEYPAD DEBUG] ⌨️ เอานิ้วกดปุ่ม: '%c' (ขณะนี้ระบบอยู่โหมด: %d)\n", key, mode);
    handleKeyPress(key);
  }

  // ── สแกนนิ้วมือ ──
  if (mode == MODE_IDLE) {
    if (fpLockoutTime > 0 && (millis() - fpLockoutTime < 30000)) {
       // แบน 30 วิ ปล่อยผ่าน
    } else {
       fpLockoutTime = 0; 

       int fpId = fingerprintScan();
       if (fpId > 0) {
         fpFailCount = 0; 
         handleFingerprintAuth(fpId); 
       } 
       else if (fpId == -2) {
         fpFailCount++;
         if (fpFailCount >= 5) {
            Serial.println("🚨 สแกนนิ้วผิด 5 ครั้ง! ล็อคระบบและสั่งถ่ายภาพ");
            fpLockoutTime = millis();
            fpFailCount = 0;
            
            lcdShow("LOCKED OUT!", "Wait 30s");
            for(int i=0; i<3; i++){ buzzerFail(); delay(200); }
            
            char eventId[32] = "";
            apiPostEvent('A', "ALERT_LOCKOUT", 0, "", 0, "Fingerprint failed 5 times");
            apiTriggerCamera('A', eventId);
            
            delay(2000);
            lcdReady();
         } else {
            lcdError("Wrong Finger");
            buzzerFail();
            delay(1500);
            lcdReady();
         }
       }
    }
  }

  delay(50);
}

// ── ตรวจ Enrollment Pending ───────────────────────────────────────────────
void checkEnrollment() {
  if (!apiIsOnline()) return;

  char   pendingUserId[32] = "";
  char   pendingName[64]   = "";
  uint8_t slot             = 0;

  bool hasPending = apiCheckPendingEnroll(pendingUserId, pendingName, slot);
  if (!hasPending) return; 

  mode = MODE_ENROLLING;
  lcdShow("Enroll:", pendingName);
  delay(1500);
  
  lcdShow("Checking Web...", "Please wait 30s");

  bool isConfirmed = false;
  bool promptShown = false; 
  unsigned long waitStart = millis();
  unsigned long lastApiCheck = millis(); 
  
  while (millis() - waitStart < 60000) { 
    if (!promptShown && (millis() - waitStart >= 30000)) {
        promptShown = true;
        lcdShow("Press # to scan", "Press * to skip");
    }

    char k = keypadGetKey();
    if (promptShown && k == '#') {
      lcdShow("Checking...", "Please wait");
      char tId[32]="", tName[64]="";
      uint8_t tSlot=0;
      if (!apiCheckPendingEnroll(tId, tName, tSlot) || strcmp(tId, pendingUserId) != 0) {
         lcdShow("Canceled", "from Web"); buzzerSuccess(); delay(1500); mode = MODE_IDLE; lcdReady(); return; 
      }
      isConfirmed = true;   
      break;
    } else if (promptShown && k == '*') {
      break; 
    }

    if (millis() - lastApiCheck >= 1500) {
      lastApiCheck = millis();
      char tId[32]="", tName[64]="";
      uint8_t tSlot=0;
      if (!apiCheckPendingEnroll(tId, tName, tSlot) || strcmp(tId, pendingUserId) != 0) {
         lcdShow("Canceled", "from Web"); buzzerSuccess(); delay(1500); mode = MODE_IDLE; lcdReady(); return; 
      }
    }
    delay(50); 
  }

  if (!isConfirmed) {
    lcdShow("Enroll Later", "Skip for now"); buzzerSuccess(); 
    apiCancelEnroll(pendingUserId);
    delay(2000); mode = MODE_IDLE; lcdReady(); return; 
  }

  lcdShow("Place finger", "on sensor");
  buzzerSuccess(); 

  if (fingerprintEnroll(slot)) {
    apiEnrollComplete(pendingUserId, slot);
    lcdShow("Enrolled!", pendingName); buzzerSuccess();
  } else {
    lcdError("Enroll fail"); buzzerFail();
  }
  delay(2000); mode = MODE_IDLE; lcdReady();
}

// ── จัดการการกด Key ────────────────────────────────────────────────────────
void handleKeyPress(char key) {
  if (mode == MODE_IDLE) {
    if (key == '1' || key == '2' || key == '3') {
      
      // 🟢 ตรวจสอบการแบน 30 วิ (ดักไว้ตั้งแต่ยังไม่ทันให้กดรหัส)
      if (globalPinLockoutTime > 0 && (millis() - globalPinLockoutTime < 30000)) {
         lcdShow("LOCKED OUT!", "Wait 30s");
         buzzerFail();
         delay(1500);
         lcdReady();
         return;
      }
      
      globalPinLockoutTime = 0; // ปลดแบน
      
      char target = (key == '1') ? 'A' : (key == '2' ? 'B' : 'C');
      pinTargetLocker = target;
      mode            = MODE_ENTER_PIN;
      pinLength       = 0;
      memset(pinBuffer, 0, sizeof(pinBuffer));
      lcdEnterPin(pinTargetLocker);
      Serial.println("[KEYPAD] เข้าสู่โหมดพิมพ์รหัส PIN แล้ว");
    }
    else if (key == '*') {
      Serial.println("[KEYPAD] กดยกเลิก กลับสู่หน้าหลัก");
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
      Serial.println("[KEYPAD] กดยกเลิก กลับสู่หน้าหลัก");
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
    lcdError("Offline"); buzzerFail(); delay(1500); lcdOffline(); return;
  }

  if (!apiVerifyFingerprint(fpId, userId, lockers)) {
    lcdError("Not found"); buzzerFail(); delay(1500); lcdReady(); return;
  }

  bool opened = false;
  const char* p = lockers;

  while (*p) {
    if (*p == 'A' || *p == 'B' || *p == 'C') {
      LockerId id = (LockerId)(*p - 'A');
      doUnlock(id, userId, "FINGERPRINT");
      opened = true;
    }
    p++;
  }

  if (!opened) {
    lcdError("No access"); buzzerFail(); delay(1500); lcdReady();
  }
}

// ── ยืนยันด้วย PIN ──────────────────────────────────────────────────────────
void handlePinAuth(char lockerId, const char* pin) {
  lcdShow("Checking PIN", "Please wait...");
  mode = MODE_IDLE;

  LockerId id = (LockerId)(lockerId - 'A');
  char userId[32] = "";

  if (!apiIsOnline()) {
    lcdError("Offline"); buzzerFail(); delay(1500); lcdOffline(); return;
  }

  if (apiVerifyPin(lockerId, pin, userId)) {
    globalPinFailCount = 0; // ตอบถูกรีเซ็ตค่า
    lockerResetFail(id); 
    doUnlock(id, userId, "PIN");
  } else {
    globalPinFailCount++;

    // 🟢 ถ้ารหัสผิดครบ 5 ครั้ง บล็อก 30 วิ ร้องเตือน และสั่งกล้องถ่ายภาพ!
    if (globalPinFailCount >= 5) {
       Serial.printf("🚨 รหัสผ่านตู้ %c ผิด 5 ครั้ง! ล็อค 30 วิ\n", lockerId);
       globalPinLockoutTime = millis();
       globalPinFailCount = 0; // รีเซ็ตเพื่อรอบถัดไป

       lcdShow("LOCKED OUT!", "Wait 30s");
       for(int i=0; i<3; i++){ buzzerFail(); delay(200); }

       char eventId[32] = "";
       apiPostEvent(lockerId, "ALERT_LOCKOUT", 0, "", 0, "PIN failed 5 times");
       apiTriggerCamera(lockerId, eventId);
       
       delay(2000);
       lcdReady();
    } else {
       lcdError("Wrong PIN");
       buzzerFail();
       apiPostEvent(lockerId, "AUTH_FAIL", 0, userId, 0, "Wrong PIN");
       delay(1500);
       lcdReady();
    }
  }

  pinLength = 0;
  memset(pinBuffer, 0, sizeof(pinBuffer));
}

// ── Unlock ตู้ ────────────────────────────────────────────────────────
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