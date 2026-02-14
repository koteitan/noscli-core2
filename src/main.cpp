#include <M5Core2.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "efontEnableJaMini.h"
#include "efont.h"
#include "../secrets.h"

#define RELAY_HOST "yabu.me"
#define RELAY_PORT 443
#define RELAY_PATH "/"

WebSocketsClient webSocket;

#define MAX_POSTS 5
String posts[MAX_POSTS];
int postCount = 0;
bool connected = false;
bool started = false;  // ボタン押すまでfalse

// efontで1文字描画、文字幅を返す
int efontDrawChar(int x, int y, uint16_t utf16, uint16_t color) {
  byte font[32];
  memset(font, 0, 32);
  getefontData(font, utf16);
  
  // 半角(8px) or 全角(16px) 判定
  bool isWide = (utf16 >= 0x100);
  int w = isWide ? 16 : 8;
  
  for (int row = 0; row < 16; row++) {
    for (int col = 0; col < w; col++) {
      int byteIndex = row * (w / 8) + col / 8;
      int bitIndex = 7 - (col % 8);
      if (font[byteIndex] & (1 << bitIndex)) {
        M5.Lcd.drawPixel(x + col, y + row, color);
      }
    }
  }
  return w;
}

// UTF-8文字列を描画（折り返し対応）、使った高さを返す
int efontDrawString(int x, int y, const String& str, uint16_t color, int maxWidth, int maxLines) {
  int cx = x;
  int cy = y;
  int line = 0;
  char* p = (char*)str.c_str();
  
  while (*p && line < maxLines) {
    uint16_t utf16;
    p = efontUFT8toUTF16(&utf16, p);
    if (utf16 == 0) continue;
    
    int charWidth = (utf16 >= 0x100) ? 16 : 8;
    
    // 折り返し
    if (cx + charWidth > x + maxWidth) {
      cx = x;
      cy += 17;
      line++;
      if (line >= maxLines) break;
    }
    
    efontDrawChar(cx, cy, utf16, color);
    cx += charWidth;
  }
  
  return (line + 1) * 17;
}

void drawHeader() {
  M5.Lcd.fillRect(0, 0, 320, 28, TFT_NAVY);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 6);
  M5.Lcd.print("ncl-core2");
  
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(210, 10);
  if (connected) {
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.print("yabu.me");
  } else {
    M5.Lcd.setTextColor(RED);
    M5.Lcd.print("disconnected");
  }
}

void drawTimeline() {
  M5.Lcd.fillRect(0, 30, 320, 210, BLACK);
  
  int y = 32;
  for (int i = 0; i < postCount && i < MAX_POSTS; i++) {
    if (i > 0) {
      M5.Lcd.drawLine(5, y - 2, 315, y - 2, TFT_DARKGREY);
    }
    
    int h = efontDrawString(5, y, posts[i], WHITE, 310, 2);
    y += h + 4;
    if (y > 230) break;
  }
}

void drawStatus(const char* msg) {
  M5.Lcd.fillRect(0, 222, 320, 18, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(5, 225);
  M5.Lcd.print(msg);
}

void sendSubscribe() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  arr.add("REQ");
  arr.add("tl");
  JsonObject filter = arr.add<JsonObject>();
  JsonArray kinds = filter["kinds"].to<JsonArray>();
  kinds.add(1);
  filter["limit"] = MAX_POSTS;
  
  String msg;
  serializeJson(doc, msg);
  webSocket.sendTXT(msg);
  
  Serial.print("Sent: ");
  Serial.println(msg);
  drawStatus("Subscribed to timeline...");
}

void handleEvent(uint8_t* payload, size_t length) {
  if (length > 4096) {
    Serial.println("Payload too large, skipping");
    return;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;
  
  if (doc[0] == "EVENT") {
    JsonObject event = doc[2];
    const char* content = event["content"];
    
    if (content) {
      String post = String(content);
      post.replace("\n", " ");
      if (post.length() > 200) {
        post = post.substring(0, 197) + "...";
      }
      
      for (int i = MAX_POSTS - 1; i > 0; i--) {
        posts[i] = posts[i - 1];
      }
      posts[0] = post;
      if (postCount < MAX_POSTS) postCount++;
      
      drawTimeline();
      
      Serial.print("Event: ");
      Serial.println(post);
    }
  } else if (doc[0] == "EOSE") {
    drawStatus("Timeline loaded!");
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      connected = false;
      drawHeader();
      drawStatus("Disconnected, reconnecting...");
      break;
      
    case WStype_CONNECTED:
      connected = true;
      drawHeader();
      sendSubscribe();
      break;
      
    case WStype_TEXT:
      handleEvent(payload, length);
      break;
      
    default:
      break;
  }
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  drawHeader();
  // 起動画面
  efontDrawString(30, 80, String("Nostr Client for M5Stack"), WHITE, 280, 1);
  efontDrawString(30, 120, String("タッチで接続開始"), GREEN, 280, 1);
  drawStatus("Waiting for touch...");
}

void startConnection() {
  started = true;
  M5.Lcd.fillRect(0, 30, 320, 190, BLACK);
  drawStatus("Connecting WiFi...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    drawStatus("WiFi FAILED! Touch to retry");
    started = false;
    return;
  }
  
  drawStatus("WiFi OK! Connecting relay...");
  
  webSocket.beginSSL(RELAY_HOST, RELAY_PORT, RELAY_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  M5.update();
  
  if (!started) {
    // タッチで接続開始
    if (M5.Touch.ispressed()) {
      startConnection();
    }
  } else {
    webSocket.loop();
  }
  yield();
}
