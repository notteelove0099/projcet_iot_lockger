// ===================================
// locker.h — จัดการลอจิกประตู
// ===================================

#pragma once
#include <Arduino.h>

enum LockerId { LOCKER_A = 0, LOCKER_B = 1, LOCKER_C = 2 };

struct LockerState {
  bool isOpen;            // เซนเซอร์บอกว่าเปิดอยู่ไหม
  bool isUnlocking;       // กำลังเปิด Relay อยู่หรือเปล่า
  unsigned long unlockTimer;
  int failCount;          // จำนวนครั้งที่กรอก PIN ผิด
  unsigned long lockoutUntil; // เวลาที่โดนล็อค
};

extern LockerState lockerStates[3];

void lockerSetup();
void lockerLoop();
void lockerUnlock(LockerId id);
bool lockerIsLockedOut(LockerId id);
void lockerRecordFail(LockerId id);
void lockerResetFail(LockerId id);