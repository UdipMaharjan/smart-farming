#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin definitions
const int trigPin = 5;
const int echoPin = 18;
const int relayPin = 26;
const int buzzerPin = 27;
const int moisturePin = 34;

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

char ssid[] = "";
char pass[] = "";

BlynkTimer timer;

#define DRY_VALUE 4095
#define WET_VALUE 2045

float currentDistance = 0;
bool manualPumpControl = false;
int moisturePercent = 0;

bool tankFullBeepActive = false;
int buzzerTimerID = -1;

// Flags to prevent repeated logEvent
bool moistureLowLogged = false;
bool moistureHighLogged = false;
bool tankFullLogged = false;
bool tankEmptyLogged = false;

void beepBuzzer() {
  static bool state = false;
  state = !state;
  digitalWrite(buzzerPin, state ? HIGH : LOW);
  Blynk.virtualWrite(V2, state ? 1 : 0);
}

void sendDHTData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Blynk.virtualWrite(V3, temperature);
  Blynk.virtualWrite(V4, humidity);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print("%");
}

void sendSoilMoisture() {
  int sensorValue = analogRead(moisturePin);
  moisturePercent = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Blynk.virtualWrite(V5, moisturePercent);

  if (!manualPumpControl) {
    if (moisturePercent < 30) {
      digitalWrite(relayPin, LOW);
      if (!moistureLowLogged) {
        Blynk.logEvent("moisture_low", "Soil moisture is low (< 30%)");
        moistureLowLogged = true;
        moistureHighLogged = false;
      }
    } else if (moisturePercent >= 75) {
      digitalWrite(relayPin, HIGH);
      if (!moistureHighLogged) {
        Blynk.logEvent("moisture_high", "Soil moisture is high (>= 75%)");
        moistureHighLogged = true;
        moistureLowLogged = false;
      }
    }
  }
}

void updateUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 20000);
  if (duration == 0) return;

  float distance = duration * 0.034 / 2.0;
  currentDistance = distance;
  Blynk.virtualWrite(V1, distance);

  if (distance < 3.0) {
    if (!tankFullBeepActive) {
      tankFullBeepActive = true;
      buzzerTimerID = timer.setInterval(500L, beepBuzzer);
      if (!tankFullLogged) {
        Blynk.logEvent("tank_full", "Tank is full (distance < 3 cm)");
        tankFullLogged = true;
        tankEmptyLogged = false;
      }
    }
  } else if (distance >= 5.0 && distance <= 8.0) {
    if (tankFullBeepActive) {
      timer.deleteTimer(buzzerTimerID);
      tankFullBeepActive = false;
    }
    digitalWrite(buzzerPin, HIGH);
    Blynk.virtualWrite(V2, 1);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    Blynk.virtualWrite(V2, 0);
    if (!tankEmptyLogged) {
      Blynk.logEvent("tank_empty", "Tank is empty");
      tankEmptyLogged = true;
      tankFullLogged = false;
    }
  } else {
    if (tankFullBeepActive) {
      timer.deleteTimer(buzzerTimerID);
      tankFullBeepActive = false;
    }
    digitalWrite(buzzerPin, LOW);
    Blynk.virtualWrite(V2, 0);
  }
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  manualPumpControl = true;
  if (value == 1) {
    digitalWrite(relayPin, LOW);
  } else {
    digitalWrite(relayPin, HIGH);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(moisturePin, INPUT);

  digitalWrite(relayPin, HIGH);
  digitalWrite(buzzerPin, LOW);

  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("....WELCOME....");

  timer.setInterval(5000L, sendDHTData);
  timer.setInterval(7000L, sendSoilMoisture);
  timer.setInterval(4000L, updateUltrasonic);
}

void loop() {
  Blynk.run();
  timer.run();
}
