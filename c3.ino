#include <WiFi.h>
#include <WebServer.h>

const char* WIFI_SSID = "Galaxy A12580F";
const char* WIFI_PASSWORD = "isreal13";

WebServer server(80);

// Hardware Pins reconfigured as requested
const int LDR_PIN = 2; 
const int RELAY_BULB = 4;
const int LDR_ON_THRESHOLD = 1500;
String commandedBulbState = "off";
unsigned long lastReconnectAttempt = 0;

void handleControl() {
  if (!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing state");
    return;
  }
  String state = server.arg("state");

  if (state != "on" && state != "off") {
    server.send(400, "text/plain", "Invalid bulb state");
    return;
  }

  if (state == "on") {
    digitalWrite(RELAY_BULB, HIGH);
  } else {
    digitalWrite(RELAY_BULB, LOW);
  }
  commandedBulbState = state;
  server.send(200, "application/json", String("{\"bulb\":\"") + commandedBulbState + "\"}");
}

void handleStatus() {
  int ldrValue = analogRead(LDR_PIN);
  String lightFeedback = (ldrValue < LDR_ON_THRESHOLD) ? "on" : "off";
  String json = String("{\"bulb\":\"") + commandedBulbState + "\",\"ldr_raw\":" + String(ldrValue) + ",\"light_feedback\":\"" + lightFeedback + "\",\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\"}";
  server.send(200, "application/json", json);
}

void handleHealth() {
  server.send(200, "application/json", "{\"ok\":true}");
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
  pinMode(RELAY_BULB, OUTPUT);
  digitalWrite(RELAY_BULB, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/health", HTTP_GET, handleHealth);
  server.begin();
}

void loop() {
  reconnectWiFi();
  server.handleClient();
}
