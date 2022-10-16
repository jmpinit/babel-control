/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp32-webupdate.local/update
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "AnchorController.h"
#include "AnchorAPI.h"
#include "wifi.h"

const float ANIMATION_BRIGHTNESS = 0.2;

const char* ssid = STASSID;
const char* password = STAPSK;

WebServer httpServer(80);
AnchorController controller;
AnchorAPI api(controller);

void handleRoot() {
  httpServer.send(200, "text/plain", "hello");
}

void animateConnecting() {
  const float spread = PI / 2;

  float angle = millis() / 100.0f;

  controller.setBrightness(0, ANIMATION_BRIGHTNESS * (1 + sin(angle)) / 2);
  controller.setBrightness(1, ANIMATION_BRIGHTNESS * (1 + sin(angle + spread)) / 2);
  controller.setBrightness(2, ANIMATION_BRIGHTNESS * (1 + sin(angle + 2 * spread)) / 2);
  controller.setBrightness(3, ANIMATION_BRIGHTNESS * (1 + sin(angle + 3 * spread)) / 2);
}

void animateConnectionSucceeded() {
  const float totalAngle = 2 * PI * 3;
  const float duration = 2000; // ms
  const float rate = 100; // Hz

  const int steps = (int)(duration / (1000.0f / rate));
  const float angleStep = totalAngle / steps;

  // 3 slow pulses
  for (float angle = 0; angle < totalAngle; angle += angleStep) {
    controller.setBrightnessAll(ANIMATION_BRIGHTNESS * (1 + sin(angle)) / 2);
    delay(1000 / rate);
  }

  controller.setBrightnessAll(0);
}

void animateConnectionFailed() {
  // 5 fast blinks
  for (int i = 0; i < 5; i++) {
    controller.setBrightnessAll(ANIMATION_BRIGHTNESS);
    delay(100);
    controller.setBrightnessAll(0);
    delay(100);
  }
}

void setup(void) {
  Serial.begin(115200);

  Serial.println();
  Serial.println("Booting Sketch...");

  controller.setup();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(api.getHostname().c_str());
  WiFi.begin(ssid, password);

  int connectStatus = WL_IDLE_STATUS;
  do {
    connectStatus = WiFi.status();

    if (connectStatus == WL_CONNECT_FAILED) {
      animateConnectionFailed();

      // Retry after a random delay to alleviate stampeding
      delay(random(3000, 6000));
      WiFi.begin(ssid, password);
    }

    animateConnecting();
    delay(10);
    Serial.print(".");
  } while (connectStatus != WL_CONNECTED);

  animateConnectionSucceeded();

  httpServer.on("/", handleRoot);

  api.setup(httpServer);

  httpServer.begin();

  Serial.printf("setup() complete");
}

void loop(void) {
  controller.update();
  httpServer.handleClient();
}
