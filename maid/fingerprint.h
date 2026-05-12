// ===================================
// fingerprint.h — AS608 Fingerprint
// ===================================

#pragma once
#include <Arduino.h>

void fingerprintSetup();

// สแกนนิ้วมือ → คืน fingerprintId (1-127), -1 ถ้าไม่เจอ
int fingerprintScan();

// ลงทะเบียนนิ้วมือ ID ที่กำหนด (ใช้โดย Admin ผ่าน Serial command)
bool fingerprintEnroll(uint8_t id);

// ลบ fingerprint ID นั้นออก
bool fingerprintDelete(uint8_t id);

// นับจำนวน fingerprint ที่ลงทะเบียนแล้ว
uint16_t fingerprintCount();
