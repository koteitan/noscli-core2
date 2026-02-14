#include <Arduino.h>

// 内蔵LED（ボードによって異なる場合あり）
#define LED_PIN 2

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("========================================");
  Serial.println("🐾 ncl-esp32 Hello World!");
  Serial.println("========================================");
  Serial.println();
  Serial.print("Chip model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Number of cores: ");
  Serial.println(ESP.getChipCores());
  Serial.print("Flash size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.println();
  Serial.println("LED blinking test started...");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("💡 LED ON");
  delay(1000);
  
  digitalWrite(LED_PIN, LOW);
  Serial.println("🌑 LED OFF");
  delay(1000);
}
