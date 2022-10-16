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

const char* ssid = STASSID;
const char* password = STAPSK;

WebServer httpServer(80);
AnchorController controller;
AnchorAPI api(controller);

void handleRoot() {
  httpServer.send(200, "text/plain", "hello");
}

void setup(void) {
  Serial.begin(115200);

  Serial.println();
  Serial.println("Booting Sketch...");

  controller.setup();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(api.getHostname().c_str());
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  httpServer.on("/", handleRoot);

  api.setup(httpServer);

  httpServer.begin();

  Serial.printf("setup() complete");
}

void loop(void) {
  controller.update();
  httpServer.handleClient();
}
