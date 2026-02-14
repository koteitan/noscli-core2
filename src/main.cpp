#include <M5Core2.h>
#include <WiFi.h>
#include "../secrets.h"

void lcdStatus(const char* msg, uint16_t color = WHITE) {
  M5.Lcd.fillRect(0, 210, 320, 30, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(color);
  M5.Lcd.setCursor(10, 215);
  M5.Lcd.print(msg);
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  
  // タイトル
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(20, 10);
  M5.Lcd.println("ncl-esp32");
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(20, 50);
  M5.Lcd.println("Nostr Client");
  M5.Lcd.setCursor(20, 75);
  M5.Lcd.println("for M5Stack Core2");
  
  // WiFi接続
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(20, 120);
  M5.Lcd.print("WiFi: ");
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(20, 145);
  M5.Lcd.print(WIFI_SSID);
  
  lcdStatus("Connecting...", YELLOW);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    // プログレスドット
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setCursor(20 + (attempts * 9), 165);
    M5.Lcd.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // 接続成功
    M5.Lcd.fillRect(0, 120, 320, 80, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setCursor(20, 120);
    M5.Lcd.print("WiFi: Connected!");
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(20, 150);
    M5.Lcd.print("IP: ");
    M5.Lcd.print(WiFi.localIP());
    
    M5.Lcd.setCursor(20, 165);
    M5.Lcd.print("RSSI: ");
    M5.Lcd.print(WiFi.RSSI());
    M5.Lcd.print(" dBm");
    
    lcdStatus("Ready!", GREEN);
    
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    // 接続失敗
    M5.Lcd.fillRect(0, 120, 320, 80, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(20, 120);
    M5.Lcd.println("WiFi: FAILED");
    
    lcdStatus("Check credentials", RED);
    
    Serial.println("WiFi connection failed!");
  }
}

void loop() {
  M5.update();
  delay(100);
}
