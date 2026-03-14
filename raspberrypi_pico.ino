#include "DHT11.h"

// ═══════════════════════════════════════════════
//  HARDWARE PINS
// ═══════════════════════════════════════════════
#define DHT1_PIN      15   // GP15 — Physical Pin 20
#define FLAME1_AO     26   // GP26 — Physical Pin 31 (ADC0)
#define FLAME2_AO     27   // GP27 — Physical Pin 32 (ADC1)

// ═══════════════════════════════════════════════
//  THRESHOLDS
// ═══════════════════════════════════════════════
#define FLAME_HIGH   1500
#define FLAME_LOW    1100
#define TEMP_CHANGE  1.0
#define HUM_CHANGE   2.0

// ═══════════════════════════════════════════════
//  SENSOR OBJECT
// ═══════════════════════════════════════════════
DHT11 dht(DHT1_PIN);

// ═══════════════════════════════════════════════
//  STATE
// ═══════════════════════════════════════════════
float lastTemp    = 0.0;
float lastHum     = 0.0;
bool  flame1State = false;
bool  flame2State = false;
int   readCount   = 0;

// ═══════════════════════════════════════════════
//  SCHMITT TRIGGER
// ═══════════════════════════════════════════════
bool schmitt(int val, bool &state, int high, int low) {
  if (val > high) state = true;
  else if (val < low) state = false;
  return state;
}

// ═══════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════
void setup() {
  // USB Serial for debugging
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  // UART1 to ESP32
  Serial1.begin(115200);

  Serial.println("================================");
  Serial.println(" Pico Sensor Node v7.0");
  Serial.println(" Sending JSON to ESP32");
  Serial.println("================================");
  delay(2000);
  Serial.println(" Ready.\n");
}

// ═══════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════
void loop() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 3000) {
    lastRead = millis();
    readCount++;

    // ── READ DHT11 ────────────────────────────
    int rawT = 0, rawH = 0;
    bool dhtOk = (dht.readTemperatureHumidity(rawT, rawH) == 0);
    if (dhtOk) {
      lastTemp = (float)rawT;
      lastHum  = (float)rawH;
    }

    // ── READ FLAME SENSORS ────────────────────
    int f1 = analogRead(FLAME1_AO);
    int f2 = analogRead(FLAME2_AO);
    schmitt(f1, flame1State, FLAME_HIGH, FLAME_LOW);
    schmitt(f2, flame2State, FLAME_HIGH, FLAME_LOW);

    // ── BUILD JSON ────────────────────────────
    String json = "{";
    json += "\"t\":"  + String(lastTemp, 1) + ",";
    json += "\"h\":"  + String(lastHum,  1) + ",";
    json += "\"f1\":" + String(f1)          + ",";
    json += "\"f2\":" + String(f2)          + ",";
    json += "\"fs1\":" + String(flame1State ? 1 : 0) + ",";
    json += "\"fs2\":" + String(flame2State ? 1 : 0);
    json += "}";

    // ── SEND TO ESP32 ─────────────────────────
    Serial1.println(json);

    // ── DEBUG TO USB SERIAL ───────────────────
    Serial.print("[#"); Serial.print(readCount); Serial.println("]");
    Serial.print("TEMP:   "); Serial.print(lastTemp, 1); Serial.println("C");
    Serial.print("HUM:    "); Serial.print(lastHum,  1); Serial.println("%");
    Serial.print("FLAME1: "); Serial.print(f1);
    Serial.println(flame1State ? " *** FIRE ***" : " safe");
    Serial.print("FLAME2: "); Serial.print(f2);
    Serial.println(flame2State ? " *** FIRE ***" : " safe");
    Serial.print("JSON:   "); Serial.println(json);
    Serial.println("--------------------------------");
  }
}