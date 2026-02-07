#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

#ifndef WIFI_SSID
#define WIFI_SSID "GamersNetwork-2.4"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "TUF7777777"
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
WebServer server(80);
bool ledOn = false;
bool filesystemReady = false;

int waterSensorValue = 4095;
bool waterLeakDetected = false;
bool waterLeakLedOn = false;
unsigned long lastWaterReadMs = 0;
unsigned long lastWaterBlinkMs = 0;

constexpr unsigned long WATER_READ_INTERVAL_MS = 250;
constexpr unsigned long WATER_BLINK_INTERVAL_MS = 200;

void writeLed(bool on) {
  ledOn = on;
  const uint8_t level = (LED_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
  digitalWrite(LED_PIN, level);
}

void writeWaterLeakLed(bool on) {
  waterLeakLedOn = on;
  digitalWrite(WATER_LED_PIN, on ? HIGH : LOW);
}

void updateWaterLeakState() {
  const unsigned long now = millis();

  if (now - lastWaterReadMs >= WATER_READ_INTERVAL_MS) {
    lastWaterReadMs = now;
    waterSensorValue = analogRead(WATER_SENSOR_PIN);
    const bool nextLeakDetected = (waterSensorValue < WATER_THRESHOLD);

    if (nextLeakDetected != waterLeakDetected) {
      waterLeakDetected = nextLeakDetected;
      Serial.printf("Vesivuoto: %s (arvo=%d, kynnys=%d)\n",
                    waterLeakDetected ? "havaittu" : "ei", waterSensorValue,
                    WATER_THRESHOLD);
      if (!waterLeakDetected) {
        writeWaterLeakLed(false);
      }
    }
  }

  if (waterLeakDetected && (now - lastWaterBlinkMs >= WATER_BLINK_INTERVAL_MS)) {
    lastWaterBlinkMs = now;
    writeWaterLeakLed(!waterLeakLedOn);
  }
}

void sendJsonStatus() {
  String json = String("{\"led\":") + (ledOn ? "true" : "false") +
                ",\"water\":{\"detected\":" +
                (waterLeakDetected ? "true" : "false") +
                ",\"value\":" + String(waterSensorValue) +
                ",\"threshold\":" + String(WATER_THRESHOLD) + "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

bool serveFromFs(const char *path, const char *contentType) {
  if (!filesystemReady) return false;
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, contentType);
  file.close();
  return true;
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

  // UI
  server.on("/", HTTP_GET, []() {
    if (serveFromFs("/index.html", "text/html; charset=utf-8")) {
      return;
    }
    server.send(
        200, "text/html; charset=utf-8",
        "<!doctype html><meta charset=utf-8><meta name=viewport "
        "content='width=device-width,initial-scale=1'>"
        "<title>ESP32 LED</title><h3>index.html puuttuu</h3>"
        "<p>Luo tiedosto <code>data/index.html</code> ja aja <code>pio run -t uploadfs</code>.</p>");
  });

  // API
  server.on("/api/status", HTTP_GET, []() { sendJsonStatus(); });

  auto handleLed = []() {
    String state = server.arg("state");
    state.toLowerCase();
    if (state == "on" || state == "1" || state == "true") {
      writeLed(true);
    } else if (state == "off" || state == "0" || state == "false") {
      writeLed(false);
    } else {
      server.send(400, "application/json",
                  "{\"error\":\"Use ?state=on|off\"}");
      return;
    }
    sendJsonStatus();
  };

  server.on("/api/led", HTTP_GET, handleLed);
  server.on("/api/led", HTTP_POST, handleLed);

  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

  server.begin();
  Serial.println("HTTP serveri kaynnissa portissa 80");
}

void loop() {
  updateWaterLeakState();
  server.handleClient();
}
