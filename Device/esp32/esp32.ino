const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char* MQTT_SERVER = "your.mqtt.broker.host";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";   
const char* MQTT_PASS = "";   
const char* DEVICE_ID = "device01";

#define BLYNK_PRINT Serial  // not used, just keep if needed

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// DHT config
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// UART pins for Serial1 (to UNO)
#define ESP32_UART_RX 16
#define ESP32_UART_TX 17

// Timing
const unsigned long DHT_INTERVAL = 10UL * 1000UL; // 10s
unsigned long lastDhtMillis = 0;

// MQTT client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Topics
String topicTelemetry;
String topicControl;
String topicEvents;
String topicMsg;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("[MQTT] recv topic: ");
  Serial.print(t);
  Serial.print(" payload: ");
  Serial.println(msg);

  // If control topic, forward raw payload to UNO (Serial1)
  if (t == topicControl) {
    Serial1.println(msg);
    Serial.print("[UART -> UNO] ");
    Serial.println(msg);
  }
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed (timeout).");
  }
}

void connectMQTT() {
  if (mqtt.connected()) return;
  Serial.print("Connecting to MQTT...");
  String clientId = String("esp32-") + DEVICE_ID + "-" + String(random(0xffff), HEX);
  bool ok;
  if (strlen(MQTT_USER) == 0) {
    ok = mqtt.connect(clientId.c_str());
  } else {
    ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);
  }
  if (ok) {
    Serial.println("connected");
    // subscribe control topic
    mqtt.subscribe(topicControl.c_str());
    Serial.print("Subscribed to: "); Serial.println(topicControl);
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqtt.state());
  }
}

void publishEvent(const char* payload) {
  if (mqtt.connected()) {
    mqtt.publish(topicEvents.c_str(), payload);
    Serial.print("[MQTT] published event: "); Serial.println(payload);
  }
}

void publishMsg(const char* payload) {
  if (mqtt.connected()) {
    mqtt.publish(topicMsg.c_str(), payload);
    Serial.print("[MQTT] published msg: "); Serial.println(payload);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32 MQTT DHT UART starting...");

  // Serial1 for UNO communication
  Serial1.begin(9600, SERIAL_8N1, ESP32_UART_RX, ESP32_UART_TX);
  Serial.println("Serial1 started (to UNO)");

  // topics
  topicTelemetry = String("matrixled/") + DEVICE_ID + "/telemetry";
  topicControl   = String("matrixled/") + DEVICE_ID + "/control";
  topicEvents    = String("matrixled/") + DEVICE_ID + "/events";
  topicMsg       = String("matrixled/") + DEVICE_ID + "/msg";

  // init DHT
  dht.begin();

  // wifi + mqtt
  connectWiFi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // small random seed
  randomSeed(analogRead(0));
}

unsigned long lastSerialCheck = 0;
String serialLine = "";

void loop() {
  // ensure WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // ensure MQTT
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();

  // DHT publish
  if (now - lastDhtMillis >= DHT_INTERVAL) {
    lastDhtMillis = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("DHT read failed");
    } else {
      // JSON telemetry
      char payload[128];
      snprintf(payload, sizeof(payload), "{\"device\":\"%s\",\"t\":%.1f,\"h\":%.1f,\"ts\":%lu}", DEVICE_ID, t, h, (unsigned long)(now/1000));
      if (mqtt.connected()) {
        mqtt.publish(topicTelemetry.c_str(), payload);
        Serial.print("[MQTT] telemetry -> "); Serial.println(payload);
      }
      // Also publish a human msg for UNO usage via MQTT (backend may choose)
      char msgBuf[64];
      snprintf(msgBuf, sizeof(msgBuf), "MSG:T:%.1fC H:%.1f%%", t, h);
      publishMsg(msgBuf);

      // Optionally, forward to UNO directly (if you want automatic display)
      // If you want ESP32 to forward telemetry to UNO (so UNO displays), uncomment:
      Serial1.println(msgBuf);
      Serial.print("[UART -> UNO] "); Serial.println(msgBuf);
    }
  }

  // Read from Serial1 (UNO -> ESP32) and publish events to MQTT
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length() > 0) {
        // publish to events topic
        String toPub = serialLine;
        toPub.trim();
        publishEvent(toPub.c_str());
        serialLine = "";
      }
    } else {
      serialLine += c;
      if (serialLine.length() > 512) serialLine = serialLine.substring(0,512);
    }
  }

  // short delay
  delay(10);
}
