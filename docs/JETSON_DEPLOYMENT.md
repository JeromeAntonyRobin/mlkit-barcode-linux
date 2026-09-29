# Deploying Google ML Kit Barhopper on NVIDIA Jetson Orin Nano

## 1. Prerequisites on Jetson Orin Nano (JetPack 5 / 6)

The Jetson Orin Nano runs Ubuntu 20.04 (JetPack 5.x) or Ubuntu 22.04 (JetPack 6.x) with an AArch64 (ARM64) CPU.

Install the necessary build tools and OpenCV:
```bash
sudo apt update
sudo apt install -y build-essential cmake python3-pip python3-numpy python3-opencv
```

---

## 2. Compiling the Shim on Device

Clone or copy this repository to your Jetson Orin Nano:
```bash
cd gmlqrkitport
bash core/build.sh
```

`core/build.sh` automatically detects the `aarch64` architecture and compiles `core/lib/arm64-v8a/libandroid_shim.so` along with the required compatibility symlinks.

---

## 3. Running with Python SDK

Run the quick verification script:
```bash
python3 google_mlkit_scanner.py
```
Or use the Python SDK inside your vision application:
```python
from sdk.python.mlkit_scanner import MLKitBarcodeScanner
import cv2

scanner = MLKitBarcodeScanner()

# Feed image from e-con Systems Jetson camera (e.g., e-CAM22_CUXVR, See3CAM)
cap = cv2.VideoCapture(0)
ret, frame = cap.read()

results = scanner.scan(frame)
for b in results:
    print(f"Format: {b['format']}, Text: {b['text']}, Corners: {b['corners']}")

scanner.close()
```

---

## 4. Running with Native C++ SDK (Maximum FPS)

To achieve lowest latency and zero-copy performance in C++ vision pipelines (V4L2, GStreamer, Argus):

```bash
cd sdk/cpp
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run benchmark demo on test samples
./mlkit_demo
```

### C++ Integration in Your Project
Link `libmlkit_scanner.so` and include `mlkit_scanner.hpp`:
```cpp
#include "mlkit_scanner.hpp"

mlkit::MLKitScanner scanner;

// Pass raw BGR buffer from camera or OpenCV Mat
auto barcodes = scanner.scanBgr(mat.data, mat.cols, mat.rows);
for (const auto& b : barcodes) {
    std::cout << "Detected: " << b.text << " [" << b.format_name << "]" << std::endl;
}
```

---

## 5. Clean Exit Note
When running the application, call `scanner.close()` followed by `os._exit(0)` in Python or `_exit(0)` in C++. This prevents Bionic and Glibc static destructor conflicts during process teardown.
