// ===================================
// api_client.cpp — HTTP ติดต่อ Server
// ===================================

#include "api_client.h"
#include "config.h"
#include "locker.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static bool _online = false;

// ── เชื่อมต่อ WiFi ────────────────────────────────────────────────────────
void apiSetup() {
  Serial.printf("เชื่อมต่อ WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _online = true;
    Serial.printf("\nเชื่อมต่อแล้ว IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    _online = false;
    Serial.println("\nไม่สามารถเชื่อมต่อ WiFi (Offline Mode)");
  }
}

bool apiIsOnline() {
  _online = (WiFi.status() == WL_CONNECTED);
  return _online;
}

// ── Helper: POST JSON ──────────────────────────────────────────────────────
static int httpPost(const char* path, const String& body, String& response) {
  if (!apiIsOnline()) return -1;

  HTTPClient http;
  String url = String(SERVER_URL) + path;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", ESP32_API_KEY);
  http.setTimeout(5000);

  int code = http.POST(body);
  if (code > 0) response = http.getString();
  http.end();
  return code;
}

// ── Heartbeat ──────────────────────────────────────────────────────────────
void apiSendHeartbeat() {
  if (!apiIsOnline()) return;

  JsonDocument doc;
  JsonArray arr = doc["lockers"].to<JsonArray>();

  const char names[3] = { 'A', 'B', 'C' };

  for (int i = 0; i < 3; i++) {
    JsonObject o = arr.add<JsonObject>();
    char idStr[2] = { names[i], '\0' };
    o["lockerId"] = idStr;

    // Magnetic Sensor: true = ประตูเปิดอยู่
    o["doorOpen"] = lockerStates[i].isOpen;

    // Weight Sensor (HX711): แค่เช็คว่ามีของหรือไม่
    o["hasItem"] = false; 
  }

  String body, resp;
  serializeJson(doc, body);
  httpPost("/api/esp32/heartbeat", body, resp);
}

// ── บันทึก Event ──────────────────────────────────────────────────────────
void apiPostEvent(char lockerId, const char* eventType,
                  int fingerprintId, const char* userId,
                  int weightGrams, const char* note) {
  JsonDocument doc;
  char idStr[2] = { lockerId, '\0' };
  doc["lockerId"]    = idStr;
  doc["eventType"]   = eventType;
  doc["weightGrams"] = weightGrams;
  doc["note"]        = note;
  if (strlen(userId) > 0) doc["userId"] = userId;

  String body, resp;
  serializeJson(doc, body);
  int code = httpPost("/api/esp32/event", body, resp);
  if (code != 200 && code != 201) {
    Serial.printf("บันทึก event ไม่สำเร็จ (HTTP %d)\n", code);
  }
}

// ── ตรวจ Fingerprint ────────────────────────────────────────────────────────
bool apiVerifyFingerprint(int fingerprintId, char* outUserId, char* outLockers) {
  JsonDocument doc;
  doc["fingerprintId"] = fingerprintId;

  String body, resp;
  serializeJson(doc, body);
  int code = httpPost("/api/esp32/verify", body, resp);

  if (code != 200) return false;

  JsonDocument res;
  if (deserializeJson(res, resp) != DeserializationError::Ok) return false;
  if (!res["found"].as<bool>()) return false;

  strlcpy(outUserId, res["userId"] | "", 32);

  String lk = "";
  for (JsonVariant v : res["assignedLockers"].as<JsonArray>()) {
    lk += v.as<String>() + ",";
  }
  strlcpy(outLockers, lk.c_str(), 16);
  return true;
}

// ── ตรวจ PIN ──────────────────────────────────────────────────────────────
bool apiVerifyPin(char lockerId, const char* pin, char* outUserId) {
  JsonDocument doc;
  char idStr[2] = { lockerId, '\0' };
  doc["lockerId"] = idStr;
  doc["pin"]      = pin;

  String body, resp;
  serializeJson(doc, body);
  int code = httpPost("/api/esp32/verify-pin", body, resp);

  if (code != 200) return false;

  JsonDocument res;
  if (deserializeJson(res, resp) != DeserializationError::Ok) return false;
  if (!res["found"].as<bool>()) return false;

  strlcpy(outUserId, res["userId"] | "", 32);
  return true;
}

// ── ตรวจ Pending Enrollment ────────────────────────────────────────────────
bool apiCheckPendingEnroll(char* outUserId, char* outName, uint8_t& outSlot) {
  if (!apiIsOnline()) return false;

  HTTPClient http;
  
  // 🟢 เคล็ดลับกัน Cache: เติม ?t=เวลาปัจจุบัน เพื่อให้ URL เปลี่ยนไปเรื่อยๆ Router จะได้ไม่จำค่าเก่า
  String url = String(SERVER_URL) + "/api/esp32/pending-enroll?t=" + String(millis());
  
  http.begin(url);
  http.addHeader("x-api-key", ESP32_API_KEY);
  http.setTimeout(3000);

  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  String resp = http.getString();
  http.end();

  JsonDocument res;
  if (deserializeJson(res, resp) != DeserializationError::Ok) return false;
  if (!res["pending"].as<bool>()) return false;

  strlcpy(outUserId, res["userId"]  | "", 32);
  strlcpy(outName,   res["name"]    | "", 64);
  outSlot = res["fingerprintSlot"].as<uint8_t>();
  return true;
}

// ── แจ้ง Server ว่าลงทะเบียนนิ้วเสร็จ ───────────────────────────────────────
void apiEnrollComplete(const char* userId, uint8_t fingerprintId) {
  JsonDocument doc;
  doc["userId"]      = userId;
  doc["fingerprintId"] = fingerprintId;

  String body, resp;
  serializeJson(doc, body);
  httpPost("/api/esp32/enroll-complete", body, resp);
}

// ── สั่ง ESP32-CAM ถ่ายภาพ ────────────────────────────────────────────────
void apiTriggerCamera(char lockerId, const char* eventId) {
  if (!apiIsOnline()) return;

  HTTPClient http;
  char url[128];
  snprintf(url, sizeof(url), "%s/capture?locker=%c&eventId=%s",
           ESP32_CAM_URL, lockerId, eventId);

  http.begin(url);
  http.setTimeout(10000); 
  
  int code = http.GET();
  http.end();

  if (code == 200) {
    Serial.printf("📷 สั่ง ESP32-CAM ถ่ายภาพช่อง %c สำเร็จ\n", lockerId);
  } else {
    Serial.printf("📷 ESP32-CAM ไม่ตอบสนอง (HTTP %d)\n", code);
  }
}

// ── ตรวจคำสั่ง Remote Unlock จาก Admin ──────────────────────────
bool apiCheckRemoteUnlock(char& outLockerId) {
  if (!apiIsOnline()) return false;

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/esp32/remote-unlock-check";
  http.begin(url);
  http.addHeader("x-api-key", ESP32_API_KEY);
  http.setTimeout(3000); 

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();

  JsonDocument res;
  if (deserializeJson(res, resp) != DeserializationError::Ok) return false;

  if (res["shouldUnlock"].as<bool>()) {
    String locker = res["lockerId"] | "A";
    outLockerId = locker[0]; 
    return true;
  }

  return false;
}

// ── แจ้ง Server ให้ยกเลิกการลงทะเบียน ───────────────────────────────────────
void apiCancelEnroll(const char* userId) {
  if (!apiIsOnline()) return;

  JsonDocument doc;
  doc["userId"] = userId;

  String body, resp;
  serializeJson(doc, body);
  httpPost("/api/esp32/cancel-enroll", body, resp);
}