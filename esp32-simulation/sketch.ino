#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

#define LCD_MAIN_ADDR 0x27
#define LCD_LIMIT_ADDR 0x26

#define DHTPIN 13
#define DHTTYPE DHT22

#define LED_PIN 2
#define SERVO_PIN 18

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "e340023a50cf44689effff5f4b6b0a62.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "esp32";
const char* mqtt_pass = "w@4$Y82+86*8^8b";

WiFiClientSecure espClient;     // TLS (Transport Layer Security) encryption over TCP
PubSubClient client(espClient);

LiquidCrystal_I2C lcdMain(LCD_MAIN_ADDR, 20, 4);
LiquidCrystal_I2C lcdLimit(LCD_LIMIT_ADDR, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Servo windowServo;

float temperature = 0;
float humidity = 0;
float temperature_limit = 30;
float humidity_limit = 80;

String windowState = "Closed";
String fanState = "Off";

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (strcmp(topic, "greenhouse/control/temp_limit") == 0) {
    temperature_limit = message.toFloat();
  }

  if (strcmp(topic, "greenhouse/control/humidity_limit") == 0) {
    humidity_limit = message.toFloat();
  }
}

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    String clientId = "ESP32-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");

      client.subscribe("greenhouse/control/temp_limit");
      client.subscribe("greenhouse/control/humidity_limit");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  lcdMain.init();
  lcdMain.backlight();
  lcdLimit.init();
  lcdLimit.backlight();

  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  windowServo.attach(SERVO_PIN);

  Wire.begin(21, 22);

  setup_wifi();

  espClient.setInsecure();     // Don’t verify the server’s TLS certificate
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT read failed");
    delay(2000);
    return;
  }

  if (temperature > temperature_limit || humidity > humidity_limit) {
    windowServo.write(90);
    digitalWrite(LED_PIN, HIGH);
    windowState = "Open";
    fanState = "On";
  } else {
    windowServo.write(0);
    digitalWrite(LED_PIN, LOW);
    windowState = "Closed";
    fanState = "Off";
  }

  String payload = "{";
  payload += "\"temperature\":" + String(temperature) + ",";
  payload += "\"humidity\":" + String(humidity) + ",";
  payload += "\"temp_limit\":" + String(temperature_limit) + ",";
  payload += "\"hum_limit\":" + String(humidity_limit) + ",";
  payload += "\"window\":\"" + windowState + "\",";
  payload += "\"fan\":\"" + fanState + "\"";
  payload += "}";

  client.publish("greenhouse/sensor1", payload.c_str());

  lcdMain.setCursor(0, 0);
  lcdMain.print("Temperature: ");
  lcdMain.print(temperature);
  lcdMain.print(" C     ");

  lcdMain.setCursor(0, 1);
  lcdMain.print("Humidity: ");
  lcdMain.print(humidity);
  lcdMain.print(" %     ");

  lcdMain.setCursor(0, 2);
  lcdMain.print("Window: ");
  lcdMain.print(windowState);
  lcdMain.print("     ");

  lcdMain.setCursor(0, 3);
  lcdMain.print("Fan: ");
  lcdMain.print(fanState);
  lcdMain.print("     ");

  lcdLimit.setCursor(0, 0);
  lcdLimit.print("Temp Lim ");
  lcdLimit.print(temperature_limit);
  lcdLimit.print(" C   ");

  lcdLimit.setCursor(0, 1);
  lcdLimit.print("Hum Lim ");
  lcdLimit.print(humidity_limit);
  lcdLimit.print(" %   ");

  Serial.println(payload);

  delay(2000);
}