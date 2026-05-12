// ===================================
// keypad.cpp — อ่าน Keypad 3x4
// ===================================

#include "my_keypad.h"
#include "config.h"
#include <Keypad.h>

// แผนผัง key ของ Keypad 3x4
static const byte ROWS = 4;
static const byte COLS = 3;
static char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

// GPIO ของ Rows และ Cols (จาก config.h)
static byte rowPins[ROWS] = KP_ROW_PINS;
static byte colPins[COLS] = KP_COL_PINS;

// Declare Keypad object pointer
static Keypad* kp = nullptr;

void keypadSetup() {
  // Initialize Keypad object
  kp = new Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
  Serial.println("Keypad ready");
}

// คืน key ที่กด หรือ 0 ถ้าไม่มีการกด
char keypadGetKey() {
  if (kp == nullptr) return 0;
  return kp->getKey();  // ไม่ blocking
}
