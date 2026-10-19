#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32-RTSPServer.h>

const char* WIFI_SSID = "Galaxy A12580F";
const char* WIFI_PASSWORD = "isreal13";

// Camera Pin Mapping (AI-Thinker Original Structure)
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

// SAFE Pin Reconfigurations
const int RELAY_FAN_S1 = 14; 
const int RELAY_FAN_S2 = 13; 
const int RELAY_FAN_S3 = 12; 

RTSPServer rtspServer;
WebServer httpServer(80); 
String currentFanState = "off";
unsigned long lastReconnectAttempt = 0;
int frameQuality = 12;

void allFansOff() {
  digitalWrite(RELAY_FAN_S1, LOW);
  digitalWrite(RELAY_FAN_S2, LOW);
  digitalWrite(RELAY_FAN_S3, LOW);
}

void handleFanControl() {
  if (!httpServer.hasArg("state")) {
    httpServer.send(400, "text/plain", "Missing state");
    return;
  }
  String state = httpServer.arg("state");

  if (state != "off" && state != "1" && state != "2" && state != "3") {
    httpServer.send(400, "text/plain", "Invalid fan state");
    return;
  }

  // Multi-relay safety interlocking check
  allFansOff();

  if (state == "1") digitalWrite(RELAY_FAN_S1, HIGH);
  else if (state == "2") digitalWrite(RELAY_FAN_S2, HIGH);
  else if (state == "3") digitalWrite(RELAY_FAN_S3, HIGH);

  currentFanState = state;
  httpServer.send(200, "application/json", String("{\"fan\":\"") + currentFanState + "\"}");
}

void handleStatus() {
  String json = String("{\"fan\":\"") + currentFanState + "\",\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\"}";
  httpServer.send(200, "application/json", json);
}

void handleHealth() {
  httpServer.send(200, "application/json", "{\"ok\":true}");
}

void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastReconnectAttempt < 5000) return;

  lastReconnectAttempt = millis();
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_FAN_S1, OUTPUT);
  pinMode(RELAY_FAN_S2, OUTPUT);
  pinMode(RELAY_FAN_S3, OUTPUT);
  allFansOff();

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // Lower resolution keeps the RTSP stream light enough for real-time gesture detection.
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t cameraStatus = esp_camera_init(&config);
  if (cameraStatus != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", cameraStatus);
  }
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    frameQuality = s->status.quality;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  if (rtspServer.init(RTSPServer::VIDEO_ONLY, 8554)) {
    Serial.println("RTSP server started on port 8554");
  } else {
    Serial.println("Failed to start RTSP server");
  }
  httpServer.on("/fan", HTTP_GET, handleFanControl);
  httpServer.on("/status", HTTP_GET, handleStatus);
  httpServer.on("/health", HTTP_GET, handleHealth);
  httpServer.begin();
}

void loop() {
  reconnectWiFi();
  httpServer.handleClient(); 
  
  if (rtspServer.readyToSendFrame()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) {
      rtspServer.sendRTSPFrame(fb->buf, fb->len, frameQuality, fb->width, fb->height);
      esp_camera_fb_return(fb);
    }
  }
  delay(1); 
}
