// ===================================
// config.h — ตั้งค่าทั้งหมด
// ===================================

#pragma once

// ── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "Tee_2.4G"
#define WIFI_PASSWORD "0817084724"

// ── Server ──────────────────────────────────────────────────────────────────
#define SERVER_URL    "http://192.168.1.39:3000"
#define ESP32_API_KEY "esp32_key_2026"

// ── Fingerprint AS608 (UART2) ─────────────────────────────────────────────
#define FP_RX_PIN  16
#define FP_TX_PIN  17

// ── LCD 16x2 I2C ──────────────────────────────────────────────────────────
#define LCD_I2C_ADDR 0x27
#define LCD_SDA_PIN  21
#define LCD_SCL_PIN  22

// ── Keypad 3x4 Matrix ────────────────────────────────────────────────────
#define KP_ROW_PINS { 32, 33, 25, 26 }
#define KP_COL_PINS { 27, 14, 12 }

// ── Relay (ควบคุมตู้) ใช้ขา 4, 5, 18 ถูกต้องแล้วครับ ─────────────────────
#define RELAY_A_PIN  4
#define RELAY_B_PIN  5
#define RELAY_C_PIN  18

// ── Magnetic Sensor (ตรวจประตูเปิด/ปิด) ─────────────────────────────────
#define MAG_A_PIN  34
#define MAG_B_PIN  35
#define MAG_C_PIN  36
#define MAG_CLOSED HIGH

// ── Buzzer (Active Buzzer) ────────────────────────────────────────────────
#define BUZZER_PIN  19

// ── Weight Sensor HX711 ─────────────────────────────────────────────────
#define HX711_A_DOUT 13
#define HX711_A_SCK  15
#define WEIGHT_THRESHOLD  500

// ── Camera ─────────────────────────────────────────────────────────────────
#define ESP32_CAM_URL  "http://192.168.1.45"

// ── การตั้งเวลา ──────────────────────────────────────────────────────────
#define HEARTBEAT_INTERVAL_MS   5000   
#define OPEN_TIMEOUT_MS        300000  
#define MAX_AUTH_FAIL           3      
#define LOCKOUT_MS             300000  
#define UNLOCK_PULSE_MS        2000    // แก้เป็น 2 วินาที (2000 ms) เพื่อให้ชัวร์