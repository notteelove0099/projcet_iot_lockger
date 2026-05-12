#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "soc/soc.h"           // สั่งปิด Brownout detector
#include "soc/rtc_cntl_reg.h"  // สั่งปิด Brownout detector

// ── 1. ตั้งค่า WiFi (ให้ตรงกับตู้หลัก) ─────────────────────────────────
const char* ssid     = "Tee_2.4G";
const char* password = "0817084724";

// ── 2. ตั้งค่า Server (Node.js) ─────────────────────────────────────
// เปลี่ยน IP ให้ตรงกับคอมพิวเตอร์ของคุณ
const char* SERVER_UPLOAD_URL = "http://192.168.1.39:3000/api/esp32/upload";
const char* ESP32_API_KEY     = "esp32_key_2026"; // ต้องตรงกับฝั่ง Server

WebServer server(80);

// ── 3. กำหนดขาของกล้องรุ่น AI-Thinker ────────────────────────────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ── ฟังก์ชันส่งรูปไปที่ Node.js Server (แก้ไขส่งแบบ Raw Binary) ────────────
void sendPhotoToServer(camera_fb_t * fb, String locker, String eventId) {
  HTTPClient http;
  
  // แนบตัวแปร lockerId และ eventId ไปต่อท้าย URL (Query String)
  String url = String(SERVER_UPLOAD_URL) + "?lockerId=" + locker + "&eventId=" + eventId;

  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("x-api-key", ESP32_API_KEY);

  // ส่งข้อมูลภาพเป็นไฟล์ดิบ (Binary) โยนทีเดียวจบ!
  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    Serial.printf("📸 ส่งรูปไป Server สำเร็จ! (HTTP %d)\n", httpResponseCode);
  } else {
    Serial.printf("❌ ส่งรูปล้มเหลว: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

// ── ฟังก์ชันรับคำสั่ง /capture จาก ESP32 หลัก ────────────────────────────
void handleCapture() {
  Serial.println("📥 ได้รับคำสั่ง /capture จากตู้หลัก...");
  
  // ดึงพารามิเตอร์ที่ตู้ส่งมาให้
  String locker = server.hasArg("locker") ? server.arg("locker") : "UNKNOWN";
  String eventId = server.hasArg("eventId") ? server.arg("eventId") : "NONE";

  // สั่งถ่ายภาพ
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ ถ่ายภาพล้มเหลว");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  // ส่งรูปลง Server
  sendPhotoToServer(fb, locker, eventId);

  // คืนค่าหน่วยความจำกล้อง
  esp_camera_fb_return(fb);

  // ตอบกลับ ESP32 หลักว่า "โอเค ถ่ายเสร็จแล้ว" (HTTP 200)
  server.send(200, "text/plain", "Capture & Upload Success");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // ป้องกันบอร์ดรีสตาร์ทเองตอนไฟกระชาก (สำคัญมากสำหรับกล้อง)
  
  Serial.begin(115200);
  Serial.println("\n\n--- ESP32-CAM Smart Locker ---");

  // ตั้งค่ากล้อง
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // ปรับคุณภาพภาพ (ย่อให้เล็กลงเพื่อให้ส่งไวขึ้น)
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; // ขนาด 640x480
    config.jpeg_quality = 12;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x", err);
    return;
  }

  // เชื่อมต่อ WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("🌐 ESP32-CAM IP: ");
  Serial.println(WiFi.localIP());

  // ตั้งค่า Web Server Router
  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("🚀 Web Server is running");
}

void loop() {
  server.handleClient(); // รอรับ HTTP Request ตลอดเวลา
}