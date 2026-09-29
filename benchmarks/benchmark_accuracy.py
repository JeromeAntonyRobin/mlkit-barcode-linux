import os
import sys
import time
import cv2

# Add repository root to path
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from sdk.python.mlkit_scanner import MLKitBarcodeScanner

def run_benchmark():
    scanner = MLKitBarcodeScanner(repo_root=REPO_ROOT)

    test_files = [
        (os.path.join(REPO_ROOT, "samples", "sample_qr.png"), "Standard QR (410x410)"),
        (os.path.join(REPO_ROOT, "samples", "sample_code128.png"), "Code 128 (506x280)"),
        (os.path.join(REPO_ROOT, "samples", "sample_qr_rotated.png"), "Rotated QR (618x618)"),
        (os.path.join(REPO_ROOT, "samples", "sample_qr_blurred.png"), "Blurred QR (410x410)")
    ]

    print("\n" + "="*70)
    print(f"{'BENCHMARK: GOOGLE ML KIT ENGINE (libbarhopper_v3.so) ON LINUX':^70}")
    print("="*70)

    for path, desc in test_files:
        if not os.path.exists(path):
            continue
        img = cv2.imread(path)

        # Warmup
        scanner.scan(img)

        # Timed iterations
        times = []
        for _ in range(20):
            t0 = time.perf_counter()
            results = scanner.scan(img)
            times.append((time.perf_counter() - t0) * 1000)

        avg_lat = sum(times) / len(times)
        p95_lat = sorted(times)[int(len(times) * 0.95)]
        fps = 1000.0 / avg_lat if avg_lat > 0 else 0

        found_str = f"Found {len(results)} barcode(s)"
        detail = ""
        if results:
            detail = f"[{results[0]['format']}] '{results[0]['text'][:35]}'"

        print(f"\n[*] Sample: {os.path.basename(path)} - {desc}")
        print(f"    Detection: {found_str} -> {detail}")
        print(f"    Latency:   Avg: {avg_lat:.2f} ms | P95: {p95_lat:.2f} ms | FPS: {fps:.1f}")

    scanner.close()
    print("\n" + "="*70)
    os._exit(0)

if __name__ == "__main__":
    run_benchmark()
