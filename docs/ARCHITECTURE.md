# Google ML Kit Barhopper on Linux: Architecture & Internals

## 1. Executive Summary

This architecture document details how Google ML Kit's proprietary Barcode & QR Code Engine (`libbarhopper_v3.so`) is executed natively on standard Linux (Ubuntu x86_64 and Ubuntu on ARM64 / NVIDIA Jetson Orin Nano) without Android containers, emulators, or virtual machines.

---

## 2. System Architecture

```
+-------------------------------------------------------------+
|             User Application (C++ or Python)                |
|             (OpenCV Mat / BGR / RGB / NV21 Frame)           |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|            SDK Layer (sdk/cpp/ or sdk/python/)              |
|   - Zero-copy buffer wrapping (Mock Android Bitmap)         |
|   - Protobuf response decoder (varint, format, corners)     |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|         Bionic Translation Shim (libandroid_shim.so)        |
|   - liblog.so        : Maps Android logcat to stdout/syslog |
|   - libjnigraphics.so: Maps AndroidBitmap to raw pointer    |
|   - JNI Mock Engine  : Simulates JVM JNIEnv and Java fields |
|   - GOT Patcher      : Hooks Bionic FILE* IO to POSIX fd 2  |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|        Google ML Kit Native Engine (libbarhopper_v3.so)      |
|   - DT_VERNEED patched ELF binary                           |
|   - MobileNet SSD Anchor Box Generator                      |
|   - Multi-scale Barcode & QR code feature extractors        |
|   - Embedded TFLite XNNPACK Neural Network execution        |
+-------------------------------------------------------------+
```

---

## 3. Core Technical Challenges & Solutions

### A. Bionic vs Glibc Symbol Incompatibilities
Android binaries link against Bionic `libc.so`, `liblog.so`, and `libjnigraphics.so`. Under Linux Glibc:
1. **Logging**: Android uses `__android_log_print` and `__android_log_vprint`. The shim intercepts these calls and routes them safely to standard streams.
2. **Graphics**: Android uses `AndroidBitmap_getInfo` and `AndroidBitmap_lockPixels`. The shim maps these to a lightweight `MockBitmap` structure pointing directly to memory buffers provided by OpenCV or V4L2.
3. **C Library Symbols**: `libbarhopper_v3.so` requires `__isoc99_sscanf`, `bsd_signal`, and standard math functions. Glibc provides these natively, with aliases where Bionic-specific naming occurs.

### B. Global Offset Table (GOT) Stdio Hooking
In Bionic libc, standard I/O handles (`stderr`, `stdout`) and `FILE` structures differ in memory layout from Glibc `FILE*`. When `libbarhopper_v3.so` attempts to write debug strings using Bionic stdio pointers, standard Glibc crashes with a segmentation fault inside `fputc`/`fwrite`.
- **Solution**: We implemented dynamic GOT patching in `got_hook.cpp`. Using `link_map` and `mprotect`, the jump slots for `fwrite`, `fflush`, `fprintf`, `vfprintf`, and `fputc` inside `libbarhopper_v3.so` are redirected to custom POSIX `write(2, ...)` and `vdprintf(2, ...)` functions.

### C. JNI Mocking & Nested Options
Google's native `Java_com_google_android_libraries_barhopper_BarhopperV3_recognizeBitmapNative` calls back into Java to read nested options:
- `multiScaleDecodingOptions`
- `multiScaleDetectionOptions`
- `onedRecognitionOptions`
- `minConsistentLines`, `extraScales`, `minDetectedDimension`

If any of these fields return `nullptr` or 0, Barhopper silently aborts detection passes. The mock `JNIEnv` intercepts `GetObjectField`, `GetIntField`, and `GetFloatField` to serve pre-configured descriptors matching Google's official default values.

### D. ELF DT_VERNEED Neutralization (ARM64 / Jetson)
Android NDK compilers inject a `DT_VERNEED` (ELF Version Needed) tag specifying `LIBC` versions (e.g. `LIBC_P` or `LIBC_PRIVATE`). Glibc's dynamic linker refuses to load the library if it finds unknown version strings.
- In `core/tools/patch_arm64_elf.py`, we patch the dynamic tag `DT_VERNEED` (tag value `0x6ffffffe`) into `DT_NULL` (tag value `0x0`). This preserves all symbols, offsets, and relocations while permitting Glibc `ld.so` to load the library cleanly.

### E. Model Weight Embedding & Protobuf Options
`barhopper_options_official.bin` bundles:
- 6 MobileNet SSD feature map layers and 36 aspect ratio anchor boxes.
- Protobuf wire streams 710 and 712 containing the 1D auto-regressor and feature extractor neural network weights (`.tflite`).
