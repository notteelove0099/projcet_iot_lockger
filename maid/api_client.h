// ===================================
// api_client.h — ติดต่อ Server
// ===================================

#pragma once
#include <Arduino.h>

void apiSetup();        
bool apiIsOnline();     

void apiSendHeartbeat();
void apiPostEvent(char lockerId, const char* eventType,
                  int fingerprintId, const char* userId,
                  int weightGrams, const char* note);

bool apiVerifyFingerprint(int fingerprintId, char* outUserId, char* outLockers);
bool apiVerifyPin(char lockerId, const char* pin, char* outUserId);

void apiTriggerCamera(char lockerId, const char* eventId);

bool apiCheckPendingEnroll(char* outUserId, char* outName, uint8_t& outSlot);
void apiEnrollComplete(const char* userId, uint8_t fingerprintId);

// ฟังก์ชันใหม่ แจ้ง Server ให้ยกเลิกการลงทะเบียน
void apiCancelEnroll(const char* userId);

bool apiCheckRemoteUnlock(char& outLockerId);