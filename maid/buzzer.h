// ===================================
// buzzer.h — ประกาศ functions Buzzer
// ===================================

#pragma once
#include <Arduino.h>

void buzzerSetup();
void buzzerSuccess();   // สำเร็จ: 1 เสียงสั้น
void buzzerFail();      // ผิดพลาด: 2 เสียงสั้น
void buzzerAlert();     // แจ้งเตือน: 3 เสียงยาว
void buzzerStop();      // หยุดเสียง
