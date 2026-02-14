# ncl-esp32

ESP32で動くnostrクライアント 🐾

## 概要

ESP32を使った軽量nostrクライアント実装です。

## 現在の状態

Hello World / LED点滅テスト

## 機能（予定）

- [ ] nostrリレーへの接続
- [ ] イベント受信
- [ ] イベント投稿
- [ ] NIP-01 基本プロトコル対応
- [ ] WiFi接続管理

## 必要なもの

- ESP32開発ボード
- PlatformIO または Arduino IDE
- USB-Cケーブル（書き込み用）

## セットアップ

### PlatformIO使用の場合

```bash
# プロジェクトをクローン
git clone https://github.com/koteitan/ncl-esp32.git
cd ncl-esp32

# ビルド＆書き込み
pio run -t upload

# シリアルモニタで確認
pio device monitor
```

### Arduino IDE使用の場合

1. `src/main.cpp` を `ncl-esp32.ino` にリネームして開く
2. ボード設定: ESP32 Dev Module
3. 書き込み速度: 115200
4. 書き込み実行

## 動作確認

- 内蔵LED（GPIO2）が1秒間隔で点滅
- シリアル出力でチップ情報とLED状態を表示

## ライセンス

MIT
