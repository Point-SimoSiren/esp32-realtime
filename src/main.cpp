#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// TÄMÄ SERVERI EI PERUSTU POLLAUKSEEN
// VAAN WEBSOCKETIIN
// HTML LÖYTYY DATA KANSIOSTA

#ifndef WIFI_SSID
#define WIFI_SSID "Joku"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Jossain"
#endif

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif

#ifndef WATER_SENSOR_PIN
#define WATER_SENSOR_PIN 34
#endif

#ifndef WATER_LED_PIN
#define WATER_LED_PIN 25
#endif

#ifndef WATER_THRESHOLD
#define WATER_THRESHOLD 1000
#endif

namespace {
AsyncWebServer server(80);
AsyncEventSource events("/api/events");
bool ledOn = false;
bool filesystemReady = false;

int waterSensorValue = 4095;
bool waterLeakDetected = false;
bool waterLeakLedOn = false;
unsigned long lastWaterReadMs = 0;
unsigned long lastWaterBlinkMs = 0;
unsigned long lastStatusPushMs = 0;
bool statusDirty = true;

constexpr unsigned long WATER_READ_INTERVAL_MS = 250;
constexpr unsigned long WATER_BLINK_INTERVAL_MS = 200;
constexpr unsigned long STATUS_PUSH_INTERVAL_MS = 1000;

String makeStatusJson() {
  return String("{\"led\":") + (ledOn ? "true" : "false") +
         ",\"water\":{\"detected\":" +
         (waterLeakDetected ? "true" : "false") +
         ",\"value\":" + String(waterSensorValue) +
         ",\"threshold\":" + String(WATER_THRESHOLD) + "}}";
}

void pushStatus(bool force = false) {
  const unsigned long now = millis();
  if (!force && !statusDirty && (now - lastStatusPushMs < STATUS_PUSH_INTERVAL_MS)) {
    return;
  }

  String json = makeStatusJson();
  events.send(json.c_str(), "status", now);
  statusDirty = false;
  lastStatusPushMs = now;
}

void writeLed(bool on) {
  if (ledOn == on) {
    return;
  }
  ledOn = on;
  const uint8_t level = (LED_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
  digitalWrite(LED_PIN, level);
  statusDirty = true;
}

void writeWaterLeakLed(bool on) {
  waterLeakLedOn = on;
  digitalWrite(WATER_LED_PIN, on ? HIGH : LOW);
}

void updateWaterLeakState() {
  const unsigned long now = millis();

  if (now - lastWaterReadMs >= WATER_READ_INTERVAL_MS) {
    lastWaterReadMs = now;
    const int previousValue = waterSensorValue;
    waterSensorValue = analogRead(WATER_SENSOR_PIN);
    const bool nextLeakDetected = (waterSensorValue < WATER_THRESHOLD);

    if (nextLeakDetected != waterLeakDetected) {
      waterLeakDetected = nextLeakDetected;
      statusDirty = true;
      Serial.printf("Vesivuoto: %s (arvo=%d, kynnys=%d)\n",
                    waterLeakDetected ? "havaittu" : "ei", waterSensorValue,
                    WATER_THRESHOLD);
      if (!waterLeakDetected) {
        writeWaterLeakLed(false);
      }
    }

    if (abs(previousValue - waterSensorValue) >= 10) {
      statusDirty = true;
    }
  }

  if (waterLeakDetected && (now - lastWaterBlinkMs >= WATER_BLINK_INTERVAL_MS)) {
    lastWaterBlinkMs = now;
    writeWaterLeakLed(!waterLeakLedOn);
  }
}

void sendJsonStatus(AsyncWebServerRequest *request) {
  String json = makeStatusJson();
  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void sendJsonError(AsyncWebServerRequest *request, int statusCode,
                   const char *message) {
  AsyncWebServerResponse *response = request->beginResponse(
      statusCode, "application/json", String("{\"error\":\"") + message + "\"}");
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  filesystemReady = LittleFS.begin();
  if (!filesystemReady) {
    Serial.println("LittleFS mount epaonnistui, yritetaan formatoida...");
    filesystemReady = LittleFS.begin(true);
  }
  Serial.printf("LittleFS: %s\n", filesystemReady ? "OK" : "EI KAYTOSSA");

  pinMode(LED_PIN, OUTPUT);
  writeLed(false);

  pinMode(WATER_LED_PIN, OUTPUT);
  writeWaterLeakLed(false);
  Serial.printf("Vesivuotoanturi: SENSOR_PIN=%d WATER_LED_PIN=%d THRESHOLD=%d\n",
                WATER_SENSOR_PIN, WATER_LED_PIN, WATER_THRESHOLD);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Yhdistetaan WiFiin SSID: %s\n", WIFI_SSID);

  const unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    updateWaterLeakState();
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32-led")) {
      Serial.println("mDNS: http://esp32-led.local/");
    }
  } else {
    Serial.println("WiFi ei yhdistynyt (20s). Tarkista WIFI_SSID/WIFI_PASSWORD.");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (filesystemReady && LittleFS.exists("/index.html")) {
      AsyncWebServerResponse *response =
          request->beginResponse(LittleFS, "/index.html", "text/html; charset=utf-8");
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
      return;
    }

    request->send(
        200, "text/html; charset=utf-8",
        "<!doctype html><meta charset=utf-8><meta name=viewport "
        "content='width=device-width,initial-scale=1'>"
        "<title>ESP32 LED</title><h3>index.html puuttuu</h3>"
        "<p>Luo tiedosto <code>data/index.html</code> ja aja <code>pio run -t uploadfs</code>.</p>");
  });

  server.on("/api/status", HTTP_GET,
            [](AsyncWebServerRequest *request) { sendJsonStatus(request); });

  auto handleLed = [](AsyncWebServerRequest *request) {
    String state;
    if (request->hasParam("state")) {
      state = request->getParam("state")->value();
    } else if (request->hasParam("state", true)) {
      state = request->getParam("state", true)->value();
    }

    state.toLowerCase();
    if (state == "on" || state == "1" || state == "true") {
      writeLed(true);
    } else if (state == "off" || state == "0" || state == "false") {
      writeLed(false);
    } else {
      sendJsonError(request, 400, "Use ?state=on|off");
      return;
    }

    sendJsonStatus(request);
    pushStatus(true);
  };

  server.on("/api/led", HTTP_GET, handleLed);
  server.on("/api/led", HTTP_POST, handleLed);

  events.onConnect([](AsyncEventSourceClient *client) {
    Serial.printf("SSE client connected: %u\n", client->lastId());
    String json = makeStatusJson();
    client->send(json.c_str(), "status", millis());
  });
  server.addHandler(&events);

  server.onNotFound(
      [](AsyncWebServerRequest *request) { request->send(404, "text/plain", "Not found"); });

  server.begin();
  Serial.println("HTTP serveri kaynnissa portissa 80");
}

void loop() {
  updateWaterLeakState();
  pushStatus();
}
