// ===================================
// lcd.cpp — ควบคุม LCD 16x2 I2C
// ===================================

#include "lcd.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// สร้าง object LCD (16 columns, 2 rows)
static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

void lcdSetup() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcdReady();
}

// แสดงข้อความ 2 บรรทัด
void lcdShow(const char* line1, const char* line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (line2 && strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void lcdClear() {
  lcd.clear();
}

void lcdReady() {
  lcdShow("Scan Finger", "or Enter PIN");
}

void lcdScanning() {
  lcdShow("Scanning...", "Please wait");
}

void lcdUnlocked(char lockerId) {
  char buf[17];
  snprintf(buf, sizeof(buf), "Unlocked [%c]", lockerId);
  lcdShow(buf, "Door is open");
}

void lcdLocked(char lockerId) {
  char buf[17];
  snprintf(buf, sizeof(buf), "Locked [%c]", lockerId);
  lcdShow(buf, "");
}

void lcdError(const char* msg) {
  lcdShow("ERROR", msg);
}

void lcdOffline() {
  lcdShow("--- OFFLINE ---", "No WiFi");
}

void lcdEnterPin(char lockerId) {
  char buf[17];
  snprintf(buf, sizeof(buf), "Locker [%c] PIN:", lockerId);
  lcdShow(buf, "");
}

void lcdLockout() {
  lcdShow("!! LOCKOUT !!", "Wait 5 min");
}
