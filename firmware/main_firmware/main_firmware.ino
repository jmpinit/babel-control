/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp32-webupdate.local/update
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "AnchorController.h"
#include "AnchorAPI.h"
#include "AnchorReceiver.h"
#include "wifi.h"

const unsigned int TIMEOUT_WIFI = 60 * 1000;
const unsigned int TIMEOUT_COMMS = 5 * 60 * 1000;
const unsigned int UDP_PORT = 4210;
const float ANIMATION_BRIGHTNESS = 0.2;
const int CONNECTION_ANIM_DURATION = PI * 1000 / 2;

const char* ssid = STASSID;
const char* password = STAPSK;

WebServer httpServer(80);
AnchorController controller;
AnchorAPI api;
AnchorReceiver receiver(UDP_PORT);

void animateConnecting() {
  // Hack to line up the animation cycles cleanly
  static unsigned long offsetTime = millis();

  const float spread = PI / 2;

  float angle = (millis() - offsetTime) / 100.0f;

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

void resetAfterDelay() {
  // Delay a random amount before resetting to help alleviate stampeding
  delay(random(30 * 1000));

  Serial.println("Resetting!");
  delay(500);
  ESP.restart();
}

void connectionWatchDog(bool updated) {
  static unsigned long timeLastConnectedMillis = millis();
  static unsigned long timeLastUpdatedMillis = millis();

  unsigned long currentMillis = millis();

  { // WiFi watchdog
    if (WiFi.status() == WL_CONNECTED) {
      // Reset the WiFi connection watchdog
      timeLastConnectedMillis = currentMillis;
    } else if ((currentMillis - timeLastConnectedMillis) > TIMEOUT_WIFI) {
      Serial.println("WiFi connection watchdog timed out. Resetting...");
      resetAfterDelay();
    }
  }

  { // Update watchdog
    if (updated) {
      // Reset the update watchdog
      timeLastUpdatedMillis = currentMillis;
    } else if ((currentMillis - timeLastUpdatedMillis) > TIMEOUT_COMMS) {
      Serial.println("Communications watchdog timed out. Resetting...");
      resetAfterDelay();
    }
  }
}

void setup(void) {
  api.setController(&controller);
  receiver.setController(&controller);

  Serial.begin(115200);

  Serial.println();
  Serial.println("Booting Sketch...");

  delay(1000);

  Serial.println("Setting up Anchor controller...");
  controller.setup();

  Serial.println("Setting up WiFi...");
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(api.getHostname().c_str());
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  unsigned long timeStartedConnecting = millis();
  int connectStatus = WL_IDLE_STATUS;
  do {
    connectStatus = WiFi.status();

    if (connectStatus == WL_CONNECT_FAILED) {
      animateConnectionFailed();
      resetAfterDelay();
    }

    // Only animate for a limited time to indicate that we are connecting
    // and then switch to a low-level default brightness to blend in
    if (millis() - timeStartedConnecting < CONNECTION_ANIM_DURATION) {
      animateConnecting();
    } else {
      controller.setBrightness(0, ANIMATION_BRIGHTNESS);
      controller.setBrightness(1, ANIMATION_BRIGHTNESS);
      controller.setBrightness(2, ANIMATION_BRIGHTNESS);
      controller.setBrightness(3, ANIMATION_BRIGHTNESS);
    }

    Serial.print(".");
    delay(10);
  } while (connectStatus != WL_CONNECTED);
  Serial.println();

  animateConnectionSucceeded();

  httpServer.on("/", [&]() {
    httpServer.send(200, "text/plain", "hello");
  });

  api.setup(httpServer);

  httpServer.begin();
  receiver.begin();

  Serial.println("setup() complete");
  Serial.print("Hostname is ");
  Serial.println(api.getHostname());
  Serial.print("IP address is ");
  Serial.println(WiFi.localIP());
}

void loop(void) {
  controller.update();
  bool updated = receiver.update();
  httpServer.handleClient();

  connectionWatchDog(updated);
}
