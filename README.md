# Google ML Kit Barcode & QR Scanner on Linux & NVIDIA Jetson

Native, container-free port of Google ML Kit's Barcode & QR Code Engine (`libbarhopper_v3.so`) for standard Linux (Ubuntu x86_64 and NVIDIA Jetson Orin Nano AArch64).

---

## Highlights

- **Zero Emulators, Zero Virtual Machines, Zero Containers**: Runs natively via a lightweight translation shim (`libandroid_shim.so`).
- **Complete Google ML Kit Vision Engine**: Executes Google's MobileNet SSD anchor box generator, multi-scale detection passes, and embedded TFLite XNNPACK models.
- **Full Barcode Support**: QR Code, Code 128, Code 39, Code 93, Data Matrix, EAN-13, EAN-8, UPC-A, UPC-E, ITF, Codabar, PDF417, and Aztec.
- **Dual APIs**:
  - **Python SDK** (`sdk/python/mlkit_scanner.py`): OpenCV BGR / RGB / NV21 integration.
  - **C++ SDK** (`sdk/cpp/include/mlkit_scanner.hpp`): Zero-copy native C++ library for maximum FPS.
- **Target Platforms**:
  - Ubuntu 20.04 / 22.04 / 24.04 (x86_64)
  - NVIDIA Jetson Orin Nano / AGX Orin / Xavier (JetPack 5.x / 6.x, ARM64)

---

## Directory Structure

```text
gmlqrkitport/
├── README.md                          # Quickstart guide & documentation
├── requirements.txt                   # Minimal Python runtime dependencies
├── google_mlkit_scanner.py            # Quick-test CLI runner
│
├── sdk/                               # Public SDK APIs
│   ├── python/
│   │   ├── mlkit_scanner.py           # Python scanner class
│   │   └── __init__.py
│   └── cpp/
│       ├── CMakeLists.txt             # Standalone C++ build system
│       ├── include/
│       │   └── mlkit_scanner.hpp      # C++ header API
│       └── src/
│           ├── mlkit_scanner.cpp      # Native C++ scanner implementation
│           └── main.cpp               # C++ test runner & benchmark demo
│
├── core/                              # Native Engine & Bionic Translation Layer
│   ├── Makefile                       # Shim compilation rules
│   ├── build.sh                       # Wrapper build script
│   ├── shim/                          # Bionic-to-Glibc translation layer source
│   │   ├── android_log_shim.cpp       # Android logcat -> stdout/syslog
│   │   ├── android_graphics_shim.cpp  # Android Bitmap -> direct memory pointer
│   │   ├── jni_mock.cpp               # Lightweight JNI mock & options generator
│   │   └── got_hook.cpp               # Dynamic Global Offset Table hook for stdio
│   ├── tools/                         # Maintenance and patching utilities
│   │   ├── generate_options_proto.py  # Generates barhopper options protobuf
│   │   └── patch_elf.py               # ELF patching & DT_VERNEED neutralizer CLI
│   └── lib/                           # Prebuilt / compiled engine binaries
│       ├── x86_64/                    # Ubuntu x86_64 binaries
│       └── arm64-v8a/                 # NVIDIA Jetson ARM64 binaries
│
├── models/                            # Google proprietary neural network models & configs
│   ├── barcode_ssd_mobilenet_v1_dmp25_quant.tflite
│   ├── oned_feature_extractor_mobile.tflite
│   ├── oned_auto_regressor_mobile.tflite
│   └── barhopper_options_official.bin
│
├── benchmarks/                        # Comparative benchmarks
│   ├── benchmark_accuracy.py          # Benchmark suite
│   └── baselines/                     # Baseline comparative scripts
│
├── samples/                           # Evaluation samples & applications
│   ├── scan_camera.py                 # Live camera / V4L2 scanner application
│   └── sample_qr.png                  # Sample test images
│
├── docs/                              # Deep-dive technical documentation
│   ├── ARCHITECTURE.md                # Architecture, GOT hooks & translation layer design
│   ├── PORTING_GUIDE.md               # Complete engineering guide & binary porting internals
│   ├── JETSON_DEPLOYMENT.md           # Instructions for Jetson Orin Nano deployment
│   └── LEGAL_AND_COMPLIANCE.md        # Commercialization, Apache 2.0 license & legal checklist
│
├── Makefile                           # Top-level orchestrator (make build, make test, etc.)
└── pyproject.toml                     # Standard Python package specification
```

---

## Quickstart

### 1. Build All Targets
To compile both the translation shim and the C++ demo binary in one step:
```bash
make build
```

### 2. Run Comprehensive Tests
Run both Python and C++ test runners against the sample barcode:
```bash
make test
```

---

## Python Usage

### Installation
Install dependencies or install the package in editable mode:
```bash
pip install -e .
```

### Run Live Camera Scanner
Scan QR codes and barcodes live from USB / CSI / V4L2 cameras with real-time OpenCV visualization:
```bash
python3 samples/scan_camera.py --camera 0
```

### Python API Example
```python
import cv2
import os
from sdk.python.mlkit_scanner import MLKitBarcodeScanner

scanner = MLKitBarcodeScanner()
frame = cv2.imread("samples/sample_qr.png")

results = scanner.scan(frame)
for b in results:
    print(f"Format:  {b['format']}")
    print(f"Text:    {b['text']}")
    print(f"Corners: {b['corners']}")

scanner.close()
os._exit(0)
```

---

## C++ Usage

### Run Pre-built Demo Binary
```bash
make run-cpp
# or directly:
./sdk/cpp/build/mlkit_demo samples/sample_qr.png

---

## Jetson Orin Nano Deployment
See [docs/JETSON_DEPLOYMENT.md](file:///home/econsystems/econ/gmlqrkitport/docs/JETSON_DEPLOYMENT.md) for step-by-step setup on JetPack 5 / 6.

---

## License & Compliance
This project is licensed under the **Apache License, Version 2.0** — see the [LICENSE](file:///home/econsystems/econ/gmlqrkitport/LICENSE) and [NOTICE](file:///home/econsystems/econ/gmlqrkitport/NOTICE) files for details.

For detailed information regarding commercial deployment, closed-source integration, and third-party attribution, refer to [docs/LEGAL_AND_COMPLIANCE.md](file:///home/econsystems/econ/gmlqrkitport/docs/LEGAL_AND_COMPLIANCE.md).
