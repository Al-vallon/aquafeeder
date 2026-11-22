/*
 * ESP32 Automatic Shrimp Feeder
 * - Wi-Fi + MQTT
 - HX711 load‑cell food level
 - Servo-driven dispenser
 - Scheduled + manual feed
 - Low‑food alert
 *
 * Documentation française : ../README.md
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <HX711.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ------------------------------------------------
// Wi‑Fi / MQTT credentials (change to yours)
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";
const char* mqtt_broker = "192.168.1.100"; // your homelab broker
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt_user";
const char* mqtt_pass = "mqtt_pass";

// ------------------------------------------------
// MQTT topics
const char* topic_food_level   = "shrimp/food/level";
const char* topic_shrimp_count = "shrimp/count";
const char* topic_feed_cmd     = "shrimp/feed/command";
const char* topic_alert        = "shrimp/alert";

// ------------------------------------------------
// Hardware pins (adjust if needed)
#define HX711_DT 18
#define HX711_SCK 19
#define SERVO_PIN 5
#define MANUAL_BTN 34

// ------------------------------------------------
// Calibration factors (HX711) – you’ll tare & determine these
#define CALIBRATION_factor 1.0f // <-- set after tare

HX711 scale;
Servo dispenser;

// ------------------------------------------------
// Time‑keeping (NTP)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // UTC+1 offset adjusted later

// ------------------------------------------------
// State
int shrimpCount = 20;               // default, can be overridden via PWA
float lowFoodThreshold = 10.0f;   // grams, configurable via PWA
bool foodLowAlert = false;
WiFiClient espClient;
PubSubClient client(espClient);

// ------------------------------------------------
void setup() {
  Serial.begin(115200);

  // --- Wi‑Fi ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("Wi‑Fi connected");

  // --- MQTT ---
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback([](char* t, byte* p, unsigned int sz){ /* ignore */ });

  // --- HX711 ---
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(CALIBRATION_factor);
  scale.tare(); // reset to 0 after loading the hopper

  // --- Servo ---
  dispenser.attach(SERVO_PIN);
  dispenser.write(0); // closed position

  // --- Manual button ---
  pinMode(MANUAL_BTN, INPUT_PULLUP);

  // --- NTP ---
  timeClient.begin();
  timeClient.setTimeOffset(3600); // adjust for your timezone (seconds)
}

// ------------------------------------------------
void loop() {
  // Keep MQTT alive
  if (!client.connected()) reconnectMqtt();
  client.loop();

  // ----- Read food level -----
  float currentLevel = scale.get_units(10); // average of 10 readings (grams)
  if (currentLevel < 0) currentLevel = 0;   // guard against negative

  // Publish food level
  char payload[8];
  dtostrf(currentLevel, 1, 2, payload);
  client.publish(topic_food_level, payload);

  // ----- Low‑food alert -----
  bool wasAlert = foodLowAlert;
  foodLowAlert = currentLevel < lowFoodThreshold;
  if (foodLowAlert && !wasAlert) {
    client.publish(topic_alert, "low_food");   // one‑shot alert
    Serial.println("🚨 Low food alert!");
  }

  // ----- Shrimp count (from MQTT) -----
  // Optional: subscribes to topic_shrimp_count if you want to adjust from PWA

  // ----- Scheduled feeding -----
  timeClient.update();
  int hour = timeClient.getHours();
  // If it's feeding time (e.g., 08:00 and 18:00) and enough food exists
  if ((hour == 8 || hour == 18) && currentLevel > lowFoodThreshold) {
    // Avoid feeding twice within the same minute
    static unsigned long lastFeed = 0;
    if (millis() - lastFeed > 60000) {
      dispenseFood();
      lastFeed = millis();
    }
  }

  // ----- Manual feed (button) -----
  if (digitalRead(MANUAL_BTN) == LOW) { // active‑low push‑button
    delay(200); // debounce
    dispenseFood();
  }

  delay(2000); // main loop rate – adjust as needed
}

// ------------------------------------------------
void dispenseFood() {
  // Rotate servo to open position, hold, then close
  dispenser.write(90);   // open (adjust to your mechanism)
  delay(2000);           // duration of dispensing
  dispenser.write(0);    // close
  // Optionally reduce food level by estimated amount
  Serial.println("🌀 Dispensed food");
}

// ------------------------------------------------
void reconnectMqtt() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Feeder", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(topic_shrimp_count); // listen for count updates
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}