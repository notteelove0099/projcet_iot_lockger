// ===================================
// lcd.h — ประกาศ functions LCD 16x2
// ===================================

#pragma once
#include <Arduino.h>

void lcdSetup();
void lcdShow(const char* line1, const char* line2 = "");
void lcdClear();

// ข้อความสำเร็จรูป
void lcdReady();          // "Scan Finger / กด PIN"
void lcdScanning();       // "กำลังอ่าน..."
void lcdUnlocked(char lockerId);   // "Unlock ช่อง X"
void lcdLocked(char lockerId);     // "ล็อค ช่อง X"
void lcdError(const char* msg);   // "Error: ..."
void lcdOffline();        // "Offline Mode"
void lcdEnterPin(char lockerId);  // "ช่อง X — กรอก PIN"
void lcdLockout();        // "ล็อค 5 นาที"
