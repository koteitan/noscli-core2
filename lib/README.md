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
