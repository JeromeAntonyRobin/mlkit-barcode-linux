import cv2
import zxingcpp
import numpy as np
import time

def generate_test_qr():
    # Create synthetic image with a QR code or barcode-like pattern if qrcode lib not installed,
    # or simple image for testing pipeline readiness
    img = np.full((480, 640, 3), 255, dtype=np.uint8)
    cv2.putText(img, "Jetson Orin Nano ML Kit Port", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 0), 2)
    return img

def benchmark_zxing_speed(iterations=50):
    img = generate_test_qr()
    # warm up
    zxingcpp.read_barcodes(img)
    
    start = time.perf_counter()
    for _ in range(iterations):
        results = zxingcpp.read_barcodes(img)
    duration = (time.perf_counter() - start) / iterations * 1000.0
    print(f"[ZXing-C++] Average scan latency on Linux: {duration:.2f} ms")

if __name__ == "__main__":
    benchmark_zxing_speed()
