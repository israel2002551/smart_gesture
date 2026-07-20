#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "src/MicroRTSP.h"

const char* ssid     = "Galaxy A12580F";
const char* password = "isreal13";

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

WiFiServer rtspServer(8554);
WebServer httpServer(80); 

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

  // Multi-relay safety interlocking check
  allFansOff();

  if (state == "1") digitalWrite(RELAY_FAN_S1, HIGH);
  else if (state == "2") digitalWrite(RELAY_FAN_S2, HIGH);
  else if (state == "3") digitalWrite(RELAY_FAN_S3, HIGH);

  httpServer.send(200, "text/plain", "Fan Updated Safely");
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
  config.frame_size = FRAMESIZE_SVGA; 
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_camera_init(&config);
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  rtspServer.begin();
  httpServer.on("/fan", HTTP_GET, handleFanControl);
  httpServer.begin();
}

void loop() {
  httpServer.handleClient(); 
  
  WiFiClient client = rtspServer.available();
  if (client) {
    MicroRTSP rtspSession(&client);
    while (client.connected()) {
      httpServer.handleClient(); 
      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) continue;
      rtspSession.writeFrame(fb->buf, fb->len);
      esp_camera_fb_return(fb);
      delay(1); 
    }
  }
}
