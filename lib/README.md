# Libraries

## libjpeg (IJG libjpeg 9f)
- Source: http://www.ijg.org/
- Decode-only (no encoder files)
- Used for **progressive JPEG** 1/8 scale decoding
- Baseline JPEG is handled by M5Stack's built-in `drawJpg`
- ESP32 porting notes:
  - `jconfig.h`: minimal config for ESP32
  - `jmorecfg.h`: modified boolean handling (Arduino defines `boolean` as `bool` 1byte, libjpeg needs `int` 4bytes — struct size mismatch causes abort)
  - `jmemnobs.c`: uses PSRAM via `heap_caps_malloc(MALLOC_CAP_SPIRAM)` for large DCT coefficient buffers
  - `main.cpp`: `#pragma push_macro("boolean")` to temporarily redefine boolean=int during jpeglib.h inclusion
  - setjmp/longjmp error handler to prevent abort() on decode failure (ESP32 reboots on abort)

## libwebp (Google libwebp 1.5.0)
- Source: https://github.com/nicestrudoc/libwebp (decode-only subset)
- Used for **WebP** image decoding with direct 32x32 scaling via `WebPDecoderConfig.options.use_scaling`
- Supports VP8, VP8L (lossless), VP8X (extended format with ICCP profiles)
- ESP32 porting notes:
  - `HAVE_CONFIG_H` defined to disable SSE/NEON auto-detection on ESP32
  - Encoder and platform-specific SIMD files excluded via `library.json` srcFilter
  - `utils.c`: uses PSRAM via `heap_caps_malloc/calloc(MALLOC_CAP_SPIRAM)` for internal decode buffers (1105x1105 images need several MB)

## Image format support summary

| Format | Method | Max size | Notes |
|---|---|---|---|
| JPEG baseline | M5 `drawJpg` + SOF scale | any | 1/1~1/8 auto-scale |
| JPEG progressive ≤1000px | libjpeg 1/8 scale | 1000x1000 | ~100KB memory |
| JPEG progressive >1000px | skip | - | DCT coeff buffer ~3MB, PSRAM fragmentation |
| PNG | Custom decoder (tinfl) | rawSize ≤4MB | Full filter reconstruction, nearest-neighbor downscale |
| PNG (large) | skip | >4MB rawSize | 2000x2000 RGBA = 16MB, exceeds PSRAM |
| WebP | libwebp direct 32x32 | any | use_scaling, PSRAM allocator |

## Chunked transfer handling
- Content-Length known: direct stream read with 10s timeout
- Content-Length unknown (chunked): `http.writeToStream()` + custom `BufStream` class
  - `getString()` is NOT binary-safe (truncates at null bytes)
  - `getStream()`/`getStreamPtr()` returns raw TCP (no chunked decoding)
  - `writeToStream()` handles chunked decoding and writes binary-safe to buffer

---

## 画像デコーダの紆余曲折

ESP32（RAM 520KB + PSRAM 4MB）の上で、Nostrのプロフィール画像をまともに表示するまでに、かなりの試行錯誤があった。以下はその記録。

### v1.2.0: 最初の実装 — 素朴なJPEGのみ

最初はM5Stack内蔵の `drawJpg()` だけでスタート。HTTPSで画像をダウンロードし、128x128のSpriteにデコード、そこから32x32にニアレストネイバーで縮小してiconPool（RGB565配列）にキャッシュ。プール20枚、メタキャッシュ100件。

問題: **JPEGしか対応してない**。NostrのプロフィールにはPNG、WebP、プログレッシブJPEGが普通にある。

### v1.3.0〜v1.3.4: 自作PNGデコーダとの格闘

#### tinflでPNGをデコードする

ESP32にはlibpngなんて載らないので、minizに同梱のtinfl（inflate専用、ヘッダのみ）を使ってPNGを自力パースすることにした。

**スタック爆発問題**: tinfl_decompressorは構造体が約11KBあり、ローカル変数に置くとESP32のタスクスタック（デフォルト8KB）を突き破ってクラッシュ。→ ヒープに `malloc` で確保して解決。

**フィルタ復元**: PNGの各行にはフィルタバイト（None/Sub/Up/Average/Paeth）がついている。全5種を実装。Paethフィルタの予測関数は地味に面倒。

**パレットPNG対応**: colorType=3（パレット画像）で bitDepth が 1/2/4/8 のいずれかの場合、ビット単位でインデックスを抽出する必要がある。PLTEチャンクのパース→パレットテーブル構築→ピクセル単位のビット演算。

**RGB565バイトスワップ**: `readPixel()` と `pushImage()` でエンディアンが違う。Spriteから読んだRGB565値をiconPoolに保存する時にバイトスワップが必要。これに気づくまでアイコンの色がめちゃくちゃだった。原因: `(px >> 8) | (px << 8)` の一行。

#### WebP対応

libwebp（Google公式）をESP32にポーティング。+80KB Flash。`use_scaling` オプションで直接32x32にスケーリングデコードできるので、メモリ効率がいい。SSE/NEONの自動検出を無効化して、エンコーダファイルを除外する `library.json` の調整が必要だった。

#### JPEG改善: SOFマーカー解析

巨大なJPEGをそのままデコードするとメモリが足りない。SOF（Start of Frame）マーカーを先読みして画像サイズを取得し、128x128以下に収まるスケール（1/1, 1/2, 1/4, 1/8）を自動選択してSpriteを再作成。

#### プログレッシブJPEG: stb_imageの導入と撤退

`drawJpg()` はベースラインJPEGしか対応していない。プログレッシブJPEGにはstb_image（+13KB Flash）を導入。SOF2マーカー（=プログレッシブ）を検出したらstb_imageにフォールバック。

しかしstb_imageは1/8スケールデコードができず、大きなプログレッシブJPEGでメモリが溢れる問題が残った。→ v1.4.0でlibjpegに置き換え。

### v1.4.0〜v1.5.0: 本格的なライブラリポーティング

#### libjpeg 9fのESP32ポーティング

プログレッシブJPEGの1/8スケールデコードのためにIJG libjpeg 9fを移植。これが一番大変だった。

**boolean型の罠**: Arduinoの `boolean` は `bool`（1バイト）、libjpegの `boolean` は `int`（4バイト）。同じ名前で違うサイズ。構造体のメンバにbooleanがあると、ヘッダで4バイトのつもりで定義した構造体が、実際には1バイトのbooleanで詰められて、メンバのオフセットがずれる。**結果: 何の前触れもなくabort()で再起動**。`#pragma push_macro` でjpeglib.hインクルード時だけbooleanをintに再定義して解決。

**abort防止**: libjpegはデコードエラー時にデフォルトで `exit()` を呼ぶ。ESP32では再起動になる。setjmp/longjmpでエラーハンドラを差し替え。

**PSRAM化**: DCT係数バッファが巨大（大きい画像で数MB）なので、jmemnobs.cのメモリアロケータをPSRAM（`heap_caps_malloc(MALLOC_CAP_SPIRAM)`）に差し替え。

#### ストリーミングPNGデコーダ（v1.5.0）

旧実装ではデコード後の全ピクセルデータ（rawBuf）をメモリに展開していた。2000x2000 RGBAだと16MB必要で、PSRAMでも無理。

tinflのリングバッファ（32KB）+ 行バッファ2本（current + previous）のストリーミング方式に書き換え。inflateの出力を行単位で消費し、フィルタ復元→即座にSpriteに描画→バッファを上書き。

**結果: 2000x2000 RGBA（元データ16MB）を67KBのメモリでデコード成功。**

#### chunked transfer encoding問題

一部の画像サーバーはContent-Lengthを返さずchunked transferで返す。

- `HTTPClient::getString()`: ぬるバイトで文字列が切断される（バイナリ非安全）
- `HTTPClient::getStream()` / `getStreamPtr()`: chunkedデコードをしない（生TCPストリーム）
- `HTTPClient::writeToStream()`: chunkedデコード対応かつバイナリ安全

カスタム `BufStream` クラスを作って `writeToStream()` の出力先にし、バイナリ安全にダウンロードを完了。

### v1.5.1: data: URI対応とペイロード制限の罠

Nostrではプロフィール画像URLに通常のHTTPS URLではなく `data:image/png;base64,...` 形式でbase64エンコードした画像を直接埋め込んでいるユーザーがいる。

data: URI対応自体は素直にmbedtlsの `mbedtls_base64_decode()` でデコードして、既存のPNG/JPEG/WebPデコーダに渡すだけ。ここは問題なかった。

**本当の問題**: WebSocketのイベント受信時に `if (length > 4096) return;` でペイロードサイズを制限していた。これは初期のWDT（ウォッチドッグタイマー）リセット対策で入れた制限。base64画像入りのkind:0イベントは5000〜6000バイトになるため、**イベントごと静かに捨てられていた**。ログにも何も出ないので原因特定に時間がかかった。

固定テスト（base64をファームウェアに直接埋め込んでデコーダ単体テスト）で「デコード失敗」という誤った結論に一度到達したが、それはテストデータのコピー時にbase64が壊れていたことが原因だった（4556文字→2716文字に欠損）。完全なデータで再テストしたところデコーダは正常動作。最終的にペイロード制限を4096→16384に拡大して解決。

**教訓**: サイレントに `return` するガード条件は危険。少なくともSerial.printfでドロップしたことをログに出すべき。

#### 切り詰めPNG対応（おまけ）

デバッグ過程で壊れたPNG（IDATチャンクが宣言サイズより短い）を扱えるように修正。デコードできた行だけ表示し、残りは黒。野良のNostrイベントには壊れた画像が流れてくることもあるので、クラッシュするよりは部分表示できた方がいい。
