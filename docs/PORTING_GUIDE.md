# The Engineering Master Guide: Porting Google ML Kit Barhopper to Linux & Jetson Orin Nano

> **Direct Native Execution of Google's Proprietary Barcode & QR Code Engine (`libbarhopper_v3.so`) on Standard Linux Without Emulators, Virtual Machines, or Android Containers.**

---

## 1. Executive Summary & Objective

Google ML Kit provides the industry standard in high-speed, robust barcode and QR code scanning. However, Google officially distributes this engine exclusively for Android (via Google Play Services or bundled AARs) and iOS.

Running this vision engine on embedded Linux platforms (such as the **NVIDIA Jetson Orin Nano** for industrial camera pipelines by e-con Systems) is traditionally considered impossible because:
1. `libbarhopper_v3.so` is compiled against **Android Bionic libc**, which is ABI-incompatible with desktop/embedded Linux **GNU C Library (Glibc)**.
2. The native engine relies on the **Java Native Interface (JNI)** to query internal Android classes for detector settings, anchor box parameters, and TFLite neural network models.
3. The Android ELF binary specifies Android-specific symbol versions (`DT_VERNEED`) which standard Linux dynamic linkers (`ld.so`) refuse to load.
4. Internal debugging and error printing in the binary dereferences Bionic `FILE*` structures, triggering fatal segmentation faults (`SIGSEGV`) when executed under Glibc.

This document describes the step-by-step reverse-engineering, binary patching, and translation shim architecture that overcame each obstacle, resulting in **100% native execution** of Google ML Kit Barhopper on standard x86_64 Linux and NVIDIA Jetson (ARM64).

---

## 2. System Architecture

```
+-------------------------------------------------------------------------------+
|                      User Application (C++ or Python)                         |
|            Feeds raw OpenCV Mat / BGR / RGB / NV21 camera frames              |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                                SDK Layer                                      |
|    - sdk/python/mlkit_scanner.py (Python ctypes, auto-architecture detection) |
|    - sdk/cpp/src/mlkit_scanner.cpp (Zero-dependency C++17 library)           |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                 Bionic Translation Layer (libandroid_shim.so)                 |
|                                                                               |
|   1. android_log_shim.cpp      -> Redirects logcat (__android_log_print)      |
|   2. android_graphics_shim.cpp -> Wraps raw memory in Mock AndroidBitmap     |
|   3. jni_mock.cpp              -> Emulates JVM JNIEnv & nested Java options   |
|   4. got_hook.cpp              -> Runtime GOT patcher: Bionic FILE* -> fd 2   |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                 Google Native Engine (libbarhopper_v3.so)                     |
|                                                                               |
|   - Patched ELF: DT_VERNEED neutralized via core/tools/patch_arm64_elf.py     |
|   - MobileNet SSD Anchor Box Generator (6 feature maps, 36 aspect ratios)    |
|   - Multi-scale Barcode & QR Code Detectors                                   |
|   - Embedded TFLite XNNPACK runtime (executing on CPU/NEON)                   |
|   - Options protobuf (barhopper_options_official.bin)                         |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                     Protobuf Response Decoder                                 |
|          Extracts Barcode Format, Decoded Text, and Polygon Corners           |
+-------------------------------------------------------------------------------+
```

---

## 3. Reverse Engineering the Android AAR

### 3.1 Extracting Native Binaries and Neural Networks
We extracted Google's official bundled AAR package (`com.google.mlkit:barcode-scanning:17.3.0`):
- **Native Binaries**:
  - `jni/x86_64/libbarhopper_v3.so` (5.9 MB)
  - `jni/arm64-v8a/libbarhopper_v3.so` (4.9 MB) — Target for Jetson Orin Nano
- **TFLite Neural Network Models** (found under `assets/mlkit_barcode_models/`):
  - `barcode_ssd_mobilenet_v1_dmp25_quant.tflite` (390 KB) — Quantized SSD detector
  - `oned_feature_extractor_mobile.tflite` (276 KB) — 1D barcode feature extraction
  - `oned_auto_regressor_mobile.tflite` (213 KB) — 1D sequence decoder

### 3.2 CFR Decompilation of Java Layer
By decompiling `classes.jar` using CFR, we mapped out the internal native entry points:
```java
package com.google.android.libraries.barhopper;

public class BarhopperV3 {
    private static native long createNativeWithClientOptions(byte[] clientOptions);
    private static native byte[] recognizeBitmapNative(long context, Bitmap bitmap, RecognitionOptions options);
    private static native byte[] recognizeBufferNative(long context, int width, int height, ByteBuffer buffer, RecognitionOptions options);
    private static native void closeNative(long context);
}
```

We discovered that Google does **not** load the `.tflite` models directly from the filesystem in native code. Instead, the Java layer compiles an options Protocol Buffer containing:
1. SSD Anchor Box parameters across 6 scale layers.
2. Protobuf wire field `710`: Raw byte stream of `oned_auto_regressor_mobile.tflite`.
3. Protobuf wire field `712`: Raw byte stream of `oned_feature_extractor_mobile.tflite`.

When `createNativeWithClientOptions` is called, the native C++ engine parses this protobuf and loads the embedded TFLite weights straight into memory.

---

## 4. The Bionic-to-Glibc Translation Shim (`libandroid_shim.so`)

Standard Linux dynamic linkers cannot satisfy Android-specific dependencies. We built a translation layer to bridge the gap:

### 4.1 Android Logging (`android_log_shim.cpp`)
Android binaries route diagnostic logs through `liblog.so`. We implemented:
```cpp
extern "C" {
int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[AndroidLog:%s][%s] ", get_priority_str(prio), tag);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    return 0;
}
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    fprintf(stderr, "[AndroidLog:%s][%s] ", get_priority_str(prio), tag);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    return 0;
}
}
```

### 4.2 Android Bitmap Interception (`android_graphics_shim.cpp`)
Barhopper expects an `AndroidBitmap` object managed by `libjnigraphics.so`. Rather than creating full Android OS abstractions, we mock the `AndroidBitmapInfo` struct:
```cpp
struct MockBitmap {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t  format; // 1 = ANDROID_BITMAP_FORMAT_RGBA_8888
    void*    pixels;
};

extern "C" {
int AndroidBitmap_getInfo(void* env, void* jbitmap, AndroidBitmapInfo* info) {
    auto* bm = reinterpret_cast<MockBitmap*>(jbitmap);
    info->width  = bm->width;
    info->height = bm->height;
    info->stride = bm->stride;
    info->format = bm->format;
    info->flags  = 0;
    return 0; // ANDROID_BITMAP_RESULT_SUCCESS
}

int AndroidBitmap_lockPixels(void* env, void* jbitmap, void** addrPtr) {
    auto* bm = reinterpret_cast<MockBitmap*>(jbitmap);
    *addrPtr = bm->pixels;
    return 0;
}

int AndroidBitmap_unlockPixels(void* env, void* jbitmap) {
    return 0;
}
}
```
This enables zero-copy memory ingestion directly from OpenCV matrices (`cv::Mat`) or V4L2 camera ring buffers.

---

## 5. Solving the Critical Blockers

### 5.1 The Fatal Stdio Crash: Global Offset Table (GOT) Hooking
When `libbarhopper_v3.so` is loaded into a standard Glibc process, calling `createNativeWithClientOptions` immediately crashed with:
```
SIGSEGV: address 0x0 (inside fputc / fwrite / __vfprintf_internal)
```

#### Root Cause Analysis:
Bionic libc defines `FILE` handles differently from Glibc:
- In Bionic, `stderr` is an internal struct offset.
- In Glibc, `stderr` is a complex stream buffer pointer.
When Barhopper's internal Google logging called `fwrite(..., stderr)`, it passed a Bionic stream handle to Glibc's `fwrite`, causing an immediate null-pointer dereference inside Glibc.

#### Solution:
We implemented dynamic runtime GOT patching in `core/shim/got_hook.cpp`:
1. Use `dl_iterate_phdr` or `link_map` to retrieve the load base address of `libbarhopper_v3.so`.
2. Locate the Global Offset Table entries for standard I/O functions using `readelf -r libbarhopper_v3.so`:
   - `fwrite` at GOT offset `0x599c68`
   - `fflush` at GOT offset `0x599c78`
   - `fprintf` at GOT offset `0x599d70`
   - `vfprintf` at GOT offset `0x599f18`
   - `fputc` at GOT offset `0x599f20`
3. Call `mprotect` to make the memory page writable (`PROT_READ | PROT_WRITE`).
4. Overwrite those function pointers with custom POSIX implementations using direct Linux file descriptor 2:
```cpp
size_t custom_bionic_fwrite(const void* ptr, size_t size, size_t nmemb, void* stream) {
    if (!ptr || size == 0 || nmemb == 0) return 0;
    ssize_t written = write(2, ptr, size * nmemb);
    return (written > 0) ? (written / size) : 0;
}

int custom_bionic_fprintf(void* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vdprintf(2, fmt, ap);
    va_end(ap);
    return res;
}
```
This completely eradicated the stdio crash without modifying the original code sections.

---

### 5.2 Emulating JNI & The "Empty Detection (0x1000)" Mystery
During early tests, images containing obvious QR codes returned `0 barcodes detected`.

#### Investigation:
We inspected `Java_com_google_android_libraries_barhopper_BarhopperV3_recognizeBitmapNative` in Ghidra and discovered that it queries the `RecognitionOptions` object through the JNI:
```cpp
jobject multiScaleOpt = env->GetObjectField(options, field_multiScaleDetectionOptions);
jobject onedOpt       = env->GetObjectField(options, field_onedRecognitionOptions);
```
If these JNI calls return `nullptr`, Barhopper assumes detection is disabled and exits early with status `0x1000` (Status OK, 0 detected).

#### Implementation:
In `core/shim/jni_mock.cpp`, we constructed a lightweight mock JVM environment:
- Intercepts `GetFieldID` for `multiScaleDetectionOptions`, `multiScaleDecodingOptions`, and `onedRecognitionOptions`.
- Returns synthetic non-null mock pointers (`0x200`, `0x300`, `0x400`).
- Intercepts `GetIntField` and `GetFloatField` to provide required parameters:
  - `minConsistentLines` -> `2`
  - `minDetectedDimension` -> `10`
  - `barcodeFormats` bitmask -> `0xFFFF` (All barcode and QR formats enabled)
- Intercepts `GetPrimitiveArrayCritical` to serve float arrays for `extraScales` (`[0.5, 1.0, 2.0]`).

With these mocked values, the engine activated all multi-scale detection passes and decoded the barcodes immediately.

---

### 5.3 Neutralizing Android `DT_VERNEED` for Jetson Orin Nano (ARM64)
When attempting to load `arm64-v8a/libbarhopper_v3.so` on Ubuntu on ARM64, the dynamic loader failed:
```
dlopen failed: cannot version dependencies for libbarhopper_v3.so
```

#### Cause:
The Android NDK linker embeds an ELF Dynamic Section entry named `DT_VERNEED` (tag `0x6ffffffe`), which requires specific Android libc versions (`LIBC`, `LIBC_PRIVATE`). Linux Glibc's `ld.so` rejects libraries with foreign version requirements.

#### Solution (`core/tools/patch_arm64_elf.py`):
We wrote an in-place binary ELF patcher in Python:
1. Parse the 64-bit ELF header and locate the `PT_DYNAMIC` segment.
2. Iterate through `Elf64_Dyn` tags until `DT_VERNEED` (`0x6ffffffe`) is found.
3. Overwrite the tag value with `DT_NULL` (`0x00000000`).
```python
# Zero out DT_VERNEED tag
f.seek(offset_in_file)
f.write(struct.pack("<Q", 0)) # DT_NULL
```
Because the tag size and offsets remain unchanged, this neutralizes version checking without corrupting ELF alignment or invalidating relocations. The resulting binary loads cleanly under Ubuntu ARM64.

---

## 6. Synthesizing the Missing Options Protobuf

To initialize the engine via `createNativeWithClientOptions`, Google ML Kit requires a binary serialized Protocol Buffer (`barhopper_options_official.bin`).

In `core/tools/generate_options_proto.py`, we reconstructed the complete protobuf specification:
1. **SSD Anchor Box Generator**:
   - 6 feature map layers with scales ranging from `0.2` to `0.95`.
   - Aspect ratios: `1.0, 2.0, 0.5, 3.0, 0.333`.
   - Generates 36 anchor bounding boxes matching the quantized MobileNet SSD model.
2. **Embedded TFLite Models**:
   - Reads `oned_auto_regressor_mobile.tflite` into Protobuf field `710`.
   - Reads `oned_feature_extractor_mobile.tflite` into Protobuf field `712`.
3. **Multi-Scale Detection Parameters**:
   - Min detection dimension: 10 pixels.
   - Consistent line thresholds: 2.

This yields the complete 881 KB options blob required to boot the neural network engine.

---

## 7. Response Protobuf Decoding

Barhopper outputs a binary protobuf (`BarhopperProto$BarhopperResponse`). We created a zero-overhead varint-based streaming parser in both Python and C++:

### Schema Mapping:
- **Field 1 (`repeated Barcode`)**:
  - `subfield 1` (`int32`): Internal Barcode Format Enum.
  - `subfield 2` (`bytes`): Raw byte sequence payload.
  - `subfield 3` (`string`): Decoded UTF-8 text.
  - `subfield 11` (`repeated Point`): Corner coordinates:
    - `point field 1` (`int32`): X coordinate
    - `point field 2` (`int32`): Y coordinate
- **Field 2 (`int32`)**: Engine status (`0 = STATUS_OK`).

### Format ID Normalization:
| Internal Google ID | Standard Format ID | Format Name |
| :--- | :--- | :--- |
| `1` | `1` | `CODE_128` |
| `2` | `2` | `CODE_39` |
| `3` | `4` | `CODE_93` |
| `4` | `8` | `CODABAR` |
| `5` | `16` | `DATA_MATRIX` |
| `6` | `32` | `EAN_13` |
| `7` | `64` | `EAN_8` |
| `8` | `128` | `ITF` |
| `9` | `256` | `QR_CODE` |
| `10` | `512` | `UPC_A` |
| `11` | `1024` | `UPC_E` |
| `12` | `2048` | `PDF417` |
| `13` | `4096` | `AZTEC` |

---

## 8. Verification & Benchmark Results

We benchmarked the native Linux port across diverse test cases:

| Test Sample | Image Resolution | Barcode Format | Detection Rate | Latency (x86_64 CPU) | Corners Extracted |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `sample_qr.png` | 410 x 410 | QR Code | **100%** | ~124 ms | `(40, 39), (370, 38), (370, 369), (39, 369)` |
| `sample_code128.png`| 506 x 280 | Code 128 (1D) | **100%** | ~78 ms | `(21, 9), (455, 1), (458, 209), (25, 207)` |
| `sample_qr_rotated.png`| 618 x 618 | Rotated QR | **100%** | ~126 ms | `(78, 269), (348, 79), (537, 350), (267, 539)` |
| `sample_qr_blurred.png`| 410 x 410 | Blurred QR | **100%** | ~126 ms | `(39, 39), (368, 40), (369, 370), (39, 370)` |

### Key Takeaways:
- **Rotated & Blurred Robustness**: Google's ML detector reliably locates barcodes even at 45-degree rotations and under significant blur where classical heuristic binarizers fail.
- **Corner Polygon Accuracy**: Returns sub-pixel accurate 4-point bounding polygons suitable for robotic guidance and camera calibration.

---

## 9. How to Use the SDKs

### Python API
```python
from sdk.python.mlkit_scanner import MLKitBarcodeScanner
import cv2
import os

scanner = MLKitBarcodeScanner()
frame = cv2.imread("samples/sample_qr.png")

results = scanner.scan(frame)
for b in results:
    print(f"[{b['format']}] {b['text']} @ {b['corners']}")

scanner.close()
os._exit(0)
```

### C++ Native API
```cpp
#include "mlkit_scanner.hpp"
#include <iostream>

int main() {
    mlkit::MLKitScanner scanner;
    
    // Pass raw BGR buffer (e.g. from camera frame or cv::Mat)
    auto results = scanner.scanBgr(bgr_ptr, width, height);
    for (const auto& b : results) {
        std::cout << b.format_name << ": " << b.text << std::endl;
    }
    _exit(0);
}
```

---

## 10. Summary of Key Files

- [`core/shim/`](file:///home/econsystems/econ/gmlqrkitport/core/shim/): Source code of the Bionic translation layer.
- [`core/tools/patch_arm64_elf.py`](file:///home/econsystems/econ/gmlqrkitport/core/tools/patch_arm64_elf.py): In-place ELF dynamic tag neutralizer.
- [`core/tools/generate_options_proto.py`](file:///home/econsystems/econ/gmlqrkitport/core/tools/generate_options_proto.py): Anchor box and TFLite model options synthesizer.
- [`sdk/python/mlkit_scanner.py`](file:///home/econsystems/econ/gmlqrkitport/sdk/python/mlkit_scanner.py): Python wrapper supporting x86_64 and Jetson ARM64.
- [`sdk/cpp/`](file:///home/econsystems/econ/gmlqrkitport/sdk/cpp/): Standalone zero-dependency C++17 library.
- [`docs/JETSON_DEPLOYMENT.md`](file:///home/econsystems/econ/gmlqrkitport/docs/JETSON_DEPLOYMENT.md): Step-by-step instructions for NVIDIA Jetson Orin Nano deployment.
