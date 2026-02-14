# noscli-core2 画像デコーダまとめ

Nostrプロフィールアイコン（32x32表示）を4つの画像形式に対応。
ESP32 (M5Stack Core2, PSRAM 8MB) の制約下で動作。

## 対応フォーマット

### JPEG ベースライン (SOF0/FFC0)
- **デコーダ**: M5Stack内蔵 TJpgDec (`sprite.drawJpg`)
- **工夫**: SOFマーカーから元サイズを解析し、1/1〜1/8スケールを自動選択。Spriteサイズをデコード後サイズに合わせて再作成し、リサンプル精度を確保
- **制約**: プログレッシブ非対応（TJpgDecの仕様）

### JPEG プログレッシブ (SOF2/FFC2)
- **デコーダ**: stb_image（ヘッダオンリーライブラリ, +13KB Flash）
- **工夫**: SOFマーカーでプログレッシブを検出し、stb_imageにフォールバック。デコード後にニアレストネイバーでSprite縮小
- **制約**: フルサイズデコードするためPSRAM必須（640x640 RGB = 1.2MB）

### PNG
- **デコーダ**: 自作（zlib inflate + フィルタ復元）
- **工夫**:
  - zlib解凍: ESP32 ROM内蔵の `tinfl_decompress` を使用。tinfl_decompressor構造体(~11KB)はヒープに確保（スタックオーバーフロー防止）
  - フィルタ復元: None/Sub/Up/Average/Paeth 全5種対応
  - カラータイプ: RGB(2), RGBA(6), グレースケール(0), パレット(3) 対応
  - パレット: PLTEチャンク読み取り。1bit/2bit/4bit/8bitインデックス対応（sub-byteビット展開）
  - 縮小: 全行フィルタ復元しつつ（Up/Paethが前行参照するため省略不可）、描画は間引き
  - 大画像もPSRAMで対応（400x400 RGBA = 640KB）
- **制約**: インターレース非対応

### WebP
- **デコーダ**: Google libwebp（デコード部のみ組み込み, +80KB Flash）
- **工夫**:
  - `WebPDecoderConfig` のスケーリング機能で直接32x32にデコード（出力バッファ ~3KB）
  - SIMD無効化（SSE2/NEON/MIPS）、ESP32用 `config.h` を手動作成
  - エンコーダ関連ソース(cost/enc/lossless_enc)を除外してビルド
- **制約**: 特になし（512x512以上はスキップ）

## 共通の工夫

### RGB565 バイトオーダー
`sprite.readPixel()` と `M5.Lcd.pushImage()` でバイトオーダーが異なる。
iconPoolに格納する際にバイトスワップ `(px >> 8) | (px << 8)` が必要。

### PSRAM活用
M5Stack Core2はPSRAM 8MB搭載。`malloc()` が自動的にPSRAMを使うため、
内部SRAMのヒープ(~90KB)では収まらない大きなバッファも確保可能。
`ESP.getFreeHeap()` ではなく `malloc` の成否で判定。

### ダウンロードサイズ制限
500KB以下のみダウンロード。それ以上はカラーブロック表示にフォールバック。

## ビルドサイズ
- Flash: ~21.3% (1.39MB / 6.55MB)
- RAM: ~2.3% (102KB / 4.52MB)
