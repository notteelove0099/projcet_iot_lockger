// ===================================
// fingerprint.cpp — ควบคุม AS608
// ===================================

#include "fingerprint.h"
#include "config.h"
#include <Adafruit_Fingerprint.h>

// UART2 สำหรับ AS608
static HardwareSerial fpSerial(2);
static Adafruit_Fingerprint finger(&fpSerial);

void fingerprintSetup() {
  fpSerial.begin(57600, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("AS608 พร้อมใช้งาน");
    Serial.printf("ลงทะเบียนแล้ว %d นิ้ว\n", finger.templateCount);
  } else {
    Serial.println("AS608 ไม่ตอบสนอง — ตรวจสอบการต่อสาย");
  }
}

// สแกนนิ้วมือ 1 ครั้ง (Non-blocking คืนทันที)
// คืน fingerprintId ถ้าเจอ, -1 ถ้าไม่มีนิ้ว, -2 ถ้าสแกนผ่านแต่ไม่ตรง, -3 ถ้า Error
int fingerprintScan() {
  // ขั้นที่ 1: อ่านภาพนิ้วมือ
  int result = finger.getImage();
  if (result == FINGERPRINT_NOFINGER) return -1;   // ไม่มีนิ้ว
  if (result != FINGERPRINT_OK)       return -3;   // อ่านภาพไม่ได้ / สัญญาณรบกวน 

  // ขั้นที่ 2: แปลงภาพเป็น template
  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) return -3;         // แปลงภาพไม่สำเร็จ

  // ขั้นที่ 3: ค้นหาใน database
  result = finger.fingerFastSearch();
  if (result == FINGERPRINT_OK) {
    Serial.printf("พบ fingerprintId: %d (confidence: %d)\n",
                  finger.fingerID, finger.confidence);
    return finger.fingerID;  // คืน ID (1–127)
  }

  return -2;  // สแกนสำเร็จ แต่ "ไม่ตรงกับใครเลย"
}

// ลงทะเบียนนิ้วมือ (ต้องสแกน 2 ครั้ง)
bool fingerprintEnroll(uint8_t id) {
  Serial.printf("เริ่มลงทะเบียน ID %d — วางนิ้วครั้งที่ 1\n", id);

  // ── รอสแกนครั้งที่ 1 ──────────────────────────────────────────────────
  while (finger.getImage() != FINGERPRINT_OK) delay(100);
  if (finger.image2Tz(1) != FINGERPRINT_OK) return false;
  Serial.println("สแกนครั้งที่ 1 สำเร็จ — ยกนิ้วออก");
  delay(2000);

  // ── รอสแกนครั้งที่ 2 ──────────────────────────────────────────────────
  Serial.println("วางนิ้วครั้งที่ 2");
  while (finger.getImage() != FINGERPRINT_OK) delay(100);
  if (finger.image2Tz(2) != FINGERPRINT_OK) return false;

  // ── สร้างและบันทึก template ───────────────────────────────────────────
  if (finger.createModel() != FINGERPRINT_OK) return false;
  if (finger.storeModel(id) != FINGERPRINT_OK) return false;

  Serial.printf("ลงทะเบียน ID %d สำเร็จ\n", id);
  return true;
}

bool fingerprintDelete(uint8_t id) {
  return finger.deleteModel(id) == FINGERPRINT_OK;
}

uint16_t fingerprintCount() {
  finger.getTemplateCount();
  return finger.templateCount;
}