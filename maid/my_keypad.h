// ===================================
// keypad.h — ประกาศ functions Keypad
// ===================================

#pragma once
#include <Arduino.h>

void keypadSetup();

// อ่าน key ที่กดในขณะนั้น (คืน 0 ถ้าไม่มี)
char keypadGetKey();
