#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "Galaxy A12580F";
const char* password = "isreal13";

WebServer server(80);

// Hardware Pins reconfigured as requested
const int LDR_PIN = 2; 
const int RELAY_BULB = 4;

void handleControl() {
  if (!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing state");
    return;
  }
  String state = server.arg("state");

  if (state == "on") {
    digitalWrite(RELAY_BULB, HIGH);
  } else {
    digitalWrite(RELAY_BULB, LOW);
  }
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  int ldrValue = analogRead(LDR_PIN);
  String lightState = (ldrValue < 1500) ? "ON" : "OFF"; 
  String json = "{\"ldr_raw\":" + String(ldrValue) + ",\"light_feedback\":\"" + lightState + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_BULB, OUTPUT);
  digitalWrite(RELAY_BULB, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();
}
