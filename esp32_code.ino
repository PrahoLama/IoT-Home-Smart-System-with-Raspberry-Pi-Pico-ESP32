#include <SH1106Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ═══════════════════════════════════════════════
//  SCREEN
// ═══════════════════════════════════════════════
SH1106Wire display(0x3C, 21, 22);

// ═══════════════════════════════════════════════
//  PINS
// ═══════════════════════════════════════════════
#define BUTTON_PIN  18
#define RX_PIN      16
#define TX_PIN      17
#define TRIG_PIN    13
#define ECHO_PIN    12
#define BUZZER_PIN  14

// ═══════════════════════════════════════════════
//  PROXIMITY
// ═══════════════════════════════════════════════
#define DETECT_DISTANCE_CM  30
#define SCREEN_TIMEOUT_MS   10000

// ═══════════════════════════════════════════════
//  BUZZER
// ═══════════════════════════════════════════════
#define BUZZER_FREQ     2000
#define BUZZER_BEEP_MS   200
#define BUZZER_PAUSE_MS  300

// ═══════════════════════════════════════════════
//  WiFi
// ═══════════════════════════════════════════════
#define WIFI_SSID "LamaBrama"
#define WIFI_PASS "19833248"

// ═══════════════════════════════════════════════
//  MQTT — HiveMQ Cloud
// ═══════════════════════════════════════════════
#define MQTT_HOST     "ee6cc7485d49430d8cf8a9101fd3b475.s1.eu.hivemq.cloud"
#define MQTT_PORT     8883
#define MQTT_USER     "pico_sensor"
#define MQTT_PASS     "Golfaina679?"  // ← add your password
#define MQTT_CLIENT   "esp32_sensor_node"
#define MQTT_TOPIC    "iot/sensors"
#define MQTT_ALERT    "iot/alerts"

// ═══════════════════════════════════════════════
//  STATE
// ═══════════════════════════════════════════════
float temp        = 0.0;
float hum         = 0.0;
int   flame1Raw   = 0;
int   flame2Raw   = 0;
bool  flame1      = false;
bool  flame2      = false;
int   menuPage    = 0;
bool  wifiOk      = false;
bool  mqttOk      = false;
bool  needsRedraw = true;
bool  picoReady   = false;
bool  screenOn    = false;
bool  buzzerOn    = false;
unsigned long lastPresence     = 0;
unsigned long lastBuzzerToggle = 0;
unsigned long lastMqttPublish  = 0;

// ═══════════════════════════════════════════════
//  BUTTON
// ═══════════════════════════════════════════════
bool          lastBtn      = HIGH;
unsigned long lastDebounce = 0;

// ═══════════════════════════════════════════════
//  MQTT CLIENT
// ═══════════════════════════════════════════════
WiFiClientSecure secureClient;
PubSubClient     mqttClient(secureClient);

// ═══════════════════════════════════════════════
//  ULTRASONIC
// ═══════════════════════════════════════════════
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

// ═══════════════════════════════════════════════
//  BUZZER
// ═══════════════════════════════════════════════
void handleBuzzer() {
  if (!flame1 && !flame2) {
    noTone(BUZZER_PIN);
    buzzerOn = false;
    return;
  }
  unsigned long now      = millis();
  unsigned long interval = buzzerOn ? BUZZER_BEEP_MS : BUZZER_PAUSE_MS;
  if (now - lastBuzzerToggle >= interval) {
    lastBuzzerToggle = now;
    if (buzzerOn) { noTone(BUZZER_PIN); buzzerOn = false; }
    else          { tone(BUZZER_PIN, BUZZER_FREQ, BUZZER_BEEP_MS); buzzerOn = true; }
  }
}

// ═══════════════════════════════════════════════
//  SCREEN ON/OFF
// ═══════════════════════════════════════════════
void screenTurnOn() {
  if (!screenOn) {
    display.displayOn();
    screenOn    = true;
    needsRedraw = true;
  }
  lastPresence = millis();
}

void screenTurnOff() {
  if (screenOn) {
    display.displayOff();
    screenOn = false;
  }
}

// ═══════════════════════════════════════════════
//  DRAW HELPERS
// ═══════════════════════════════════════════════
void drawThermometer(int x, int y) {
  display.drawRect(x + 3, y, 9, 22);
  display.fillCircle(x + 7, y + 24, 7);
  display.fillRect(x + 5, y + 10, 5, 14);
  display.drawLine(x + 12, y + 6,  x + 14, y + 6);
  display.drawLine(x + 12, y + 12, x + 14, y + 12);
  display.drawLine(x + 12, y + 18, x + 14, y + 18);
}

void drawDroplet(int x, int y) {
  display.fillCircle(x + 7, y + 20, 7);
  display.fillTriangle(x + 7, y + 2, x + 1, y + 16, x + 13, y + 16);
}

void drawFlame(int x, int y) {
  display.fillTriangle(x + 8, y, x, y + 22, x + 16, y + 22);
  display.setColor(BLACK);
  display.fillTriangle(x + 8, y + 8, x + 3, y + 22, x + 13, y + 22);
  display.setColor(WHITE);
  display.fillRect(x + 1, y + 21, 14, 4);
}

void drawPageDots(int page) {
  for (int i = 0; i < 3; i++) {
    if (i == page) display.fillCircle(55 + (i * 10), 61, 2);
    else           display.drawCircle(55 + (i * 10), 61, 2);
  }
}

void drawStatus() {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setColor(BLACK);
  String s = "";
  s += wifiOk    ? "W" : "-";
  s += mqttOk    ? "M" : "-";
  s += picoReady ? "P" : "-";
  display.drawString(127, 1, s);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

// ═══════════════════════════════════════════════
//  SCREENS
// ═══════════════════════════════════════════════
void showBootScreen(const char* line1, const char* line2) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 16, line1);
  display.drawString(64, 32, line2);
  display.display();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void showTempPage() {
  display.clear();
  display.fillRect(0, 0, 128, 13);
  display.setColor(BLACK);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60, 1, "TEMPERATURE");
  drawStatus();
  display.setColor(WHITE);
  drawThermometer(2, 17);
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(28, 16, (temp == 0.0) ? "--.-" : String(temp, 1));
  display.setFont(ArialMT_Plain_10);
  display.drawString(100, 16, "oC");
  display.drawLine(0, 43, 128, 43);
  display.drawString(2, 46, "Hum: " + String(hum, 1) + "%");
  drawPageDots(0);
  display.display();
}

void showHumPage() {
  display.clear();
  display.fillRect(0, 0, 128, 13);
  display.setColor(BLACK);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60, 1, "HUMIDITY");
  drawStatus();
  display.setColor(WHITE);
  drawDroplet(2, 17);
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(28, 16, (hum == 0.0) ? "--.-" : String(hum, 1));
  display.setFont(ArialMT_Plain_10);
  display.drawString(104, 16, "%");
  display.drawLine(0, 43, 128, 43);
  display.drawString(2, 46, "Temp: " + String(temp, 1) + "C");
  drawPageDots(1);
  display.display();
}

void showFlamePage() {
  display.clear();
  display.fillRect(0, 0, 128, 13);
  display.setColor(BLACK);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(60, 1, "FLAME");
  drawStatus();
  display.setColor(WHITE);
  if (flame1 || flame2) {
    display.drawRect(0, 0, 128, 64);
    display.drawRect(2, 2, 124, 60);
    drawFlame(56, 14);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 42, "!! FIRE DETECTED !!");
    if (flame1 && flame2)
      display.drawString(64, 53, "Both sensors active!");
    else
      display.drawString(64, 53, "Sensor " + String(flame1 ? "1" : "2") + " triggered!");
  } else {
    drawFlame(2, 16);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(24, 16, "S1: " + String(flame1Raw));
    int b1 = map(constrain(flame1Raw, 0, 4095), 0, 4095, 0, 46);
    display.drawRect(80, 16, 46, 7);
    display.fillRect(80, 16, b1, 7);
    display.drawString(24, 30, "S2: " + String(flame2Raw));
    int b2 = map(constrain(flame2Raw, 0, 4095), 0, 4095, 0, 46);
    display.drawRect(80, 30, 46, 7);
    display.fillRect(80, 30, b2, 7);
    display.drawString(24, 44, "Status: SAFE");
    drawPageDots(2);
  }
  display.display();
}

void showWaitingScreen(int dots) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 10, "IoT Node v7.0");
  display.drawString(64, 26, "Waiting for Pico");
  String dotStr = "";
  for (int i = 0; i < dots; i++) dotStr += ".";
  display.drawString(64, 40, dotStr);
  display.display();
}

// ═══════════════════════════════════════════════
//  MQTT PUBLISH
// ═══════════════════════════════════════════════
void mqttPublish() {
  if (!mqttClient.connected()) return;

  // Sensor data
  JsonDocument doc;
  doc["t"]   = temp;
  doc["h"]   = hum;
  doc["f1"]  = flame1Raw;
  doc["f2"]  = flame2Raw;
  doc["fs1"] = flame1 ? 1 : 0;
  doc["fs2"] = flame2 ? 1 : 0;

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(MQTT_TOPIC, payload.c_str());
  Serial.print("MQTT published: ");
  Serial.println(payload);

  // Fire alert
  if (flame1 || flame2) {
    JsonDocument alert;
    alert["alert"] = "FIRE";
    alert["s1"]    = flame1 ? 1 : 0;
    alert["s2"]    = flame2 ? 1 : 0;
    String alertPayload;
    serializeJson(alert, alertPayload);
    mqttClient.publish(MQTT_ALERT, alertPayload.c_str());
    Serial.println("MQTT FIRE ALERT sent!");
  }
}

// ═══════════════════════════════════════════════
//  MQTT RECONNECT
// ═══════════════════════════════════════════════
void mqttReconnect() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("MQTT connecting...");
  if (mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
    mqttOk = true;
    needsRedraw = true;
    Serial.println(" connected!");
    // Boot beep confirmation
    tone(BUZZER_PIN, 1500, 100);
  } else {
    mqttOk = false;
    Serial.print(" failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// ═══════════════════════════════════════════════
//  PARSE JSON FROM PICO
// ═══════════════════════════════════════════════
void parseJSON(String raw) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, raw);
  if (err) return;

  temp      = doc["t"]   | 0.0f;
  hum       = doc["h"]   | 0.0f;
  flame1Raw = doc["f1"]  | 0;
  flame2Raw = doc["f2"]  | 0;
  flame1    = doc["fs1"] | 0;
  flame2    = doc["fs2"] | 0;

  if (flame1 || flame2) {
    screenTurnOn();
    menuPage = 2;
  }

  if (!picoReady) {
    picoReady = true;
    Serial.println("Pico connected!");
  }

  // Publish to MQTT every 5 seconds
  if (millis() - lastMqttPublish > 5000) {
    lastMqttPublish = millis();
    mqttPublish();
  }

  needsRedraw = true;
}

// ═══════════════════════════════════════════════
//  WIFI
// ═══════════════════════════════════════════════
void connectWiFi() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  showBootScreen("Connecting WiFi...", WIFI_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
    showBootScreen("WiFi Connected!", WiFi.localIP().toString().c_str());
  } else {
    wifiOk = false;
    showBootScreen("WiFi failed!", "Offline mode");
  }
  delay(1500);
}

// ═══════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  // OLED
  display.init();
  display.setI2cAutoInit(false);
  Wire.begin(21, 22);
  Wire.setClock(700000);
  display.flipScreenVertically();
  display.displayOn();
  display.clear();
  display.display();
  delay(100);

  // Boot beep
  tone(BUZZER_PIN, 1000, 100);
  delay(150);
  tone(BUZZER_PIN, 2000, 100);
  delay(150);
  noTone(BUZZER_PIN);

  showBootScreen("IoT Node v7.0", "Booting...");
  delay(1500);

  // WiFi
  connectWiFi();

  // MQTT setup
  secureClient.setInsecure();  // skip cert verification for now
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  showBootScreen("Connecting MQTT...", MQTT_HOST);
  mqttReconnect();
  delay(1500);

  // Wait for Pico
  unsigned long waitStart = millis();
  int dots = 0;
  while (!picoReady && millis() - waitStart < 15000) {
    if (Serial2.available()) {
      String line = Serial2.readStringUntil('\n');
      line.trim();
      if (line.startsWith("{")) parseJSON(line);
    }
    if (millis() % 500 < 10) {
      dots = (dots + 1) % 4;
      showWaitingScreen(dots);
    }
    delay(10);
  }

  if (!picoReady) {
    showBootScreen("Pico not found!", "Check UART wires");
    delay(2000);
  }

  screenOn = false;
  display.displayOff();
  lastPresence = millis();
  needsRedraw  = true;
  Serial.println("Ready!");
}

// ═══════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════
void loop() {

  // ── MQTT KEEP ALIVE ──────────────────────────
  if (wifiOk) {
    if (!mqttClient.connected()) {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 5000) {
        lastReconnect = millis();
        mqttReconnect();
      }
    }
    mqttClient.loop();
  }

  // ── UART FROM PICO ───────────────────────────
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.startsWith("{")) parseJSON(line);
  }

  // ── BUZZER ───────────────────────────────────
  handleBuzzer();

  // ── PROXIMITY ────────────────────────────────
  static unsigned long lastScan = 0;
  if (millis() - lastScan > 200) {
    lastScan = millis();
    float dist = getDistance();
    if (dist < DETECT_DISTANCE_CM) {
      screenTurnOn();
    } else if (screenOn && millis() - lastPresence > SCREEN_TIMEOUT_MS) {
      screenTurnOff();
    }
  }

  // ── BUTTON ───────────────────────────────────
  if (screenOn) {
    bool reading = digitalRead(BUTTON_PIN);
    if (reading == LOW && lastBtn == HIGH) {
      unsigned long now = millis();
      if (now - lastDebounce > 200) {
        lastDebounce = now;
        menuPage     = (menuPage + 1) % 3;
        needsRedraw  = true;
        lastPresence = millis();
      }
    }
    lastBtn = reading;
  }

  // ── REDRAW ───────────────────────────────────
  if (needsRedraw && screenOn) {
    switch (menuPage) {
      case 0: showTempPage();  break;
      case 1: showHumPage();   break;
      case 2: showFlamePage(); break;
    }
    needsRedraw = false;
  }

  delay(10);
}
