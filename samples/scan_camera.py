#!/usr/bin/env python3
"""
Live Camera / Video Stream Barcode Scanner Demo
Uses OpenCV VideoCapture to stream from a V4L2 device, CSI camera, or RTSP stream
and performs real-time Google ML Kit barcode decoding.
"""

import sys
import time
import argparse
import cv2

from sdk.python.mlkit_scanner import MLKitBarcodeScanner


def run_camera_stream(device_id=0, width=1280, height=720):
    print("=" * 65)
    print(f"[*] Initializing Camera Device: {device_id} ({width}x{height})")
    print("=" * 65)

    # Initialize video capture (supports camera index or video file path)
    if isinstance(device_id, str) and device_id.isdigit():
        device_id = int(device_id)

    cap = cv2.VideoCapture(device_id)
    if not cap.isOpened():
        print(f"[-] Error: Could not open video device {device_id}")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

    scanner = MLKitBarcodeScanner()
    print("[+] Scanner ready. Press 'q' or ESC in window to exit.")

    fps_count = 0
    start_time = time.time()
    fps_display = "FPS: 0"

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("[-] End of stream or dropped frame.")
                break

            t0 = time.time()
            barcodes = scanner.scan(frame)
            latency_ms = (time.time() - t0) * 1000

            # Draw detections
            for b in barcodes:
                corners = b.get("corners", [])
                text = b.get("text", "")
                fmt = b.get("format", "")

                if len(corners) == 4:
                    pts = [(int(p[0]), int(p[1])) for p in corners]
                    for i in range(4):
                        cv2.line(frame, pts[i], pts[(i + 1) % 4], (0, 255, 0), 2)
                    cv2.putText(
                        frame,
                        f"[{fmt}] {text}",
                        (pts[0][0], max(20, pts[0][1] - 10)),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.6,
                        (0, 255, 0),
                        2,
                    )
                    print(f"  -> [{fmt}] '{text}' ({latency_ms:.1f}ms)")

            # Calculate FPS
            fps_count += 1
            if time.time() - start_time >= 1.0:
                fps_display = f"FPS: {fps_count:.1f} | Latency: {latency_ms:.1f}ms"
                fps_count = 0
                start_time = time.time()

            cv2.putText(
                frame,
                fps_display,
                (20, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (0, 255, 255),
                2,
            )

            cv2.imshow("Google ML Kit Camera Scanner (Press Q to Exit)", frame)
            if cv2.waitKey(1) & 0xFF in (ord("q"), 27):
                break

    finally:
        cap.release()
        cv2.destroyAllWindows()
        scanner.close()
        print("[+] Stream closed successfully.")


def main():
    parser = argparse.ArgumentParser(description="Live camera stream barcode scanner using Google ML Kit")
    parser.add_argument("--device", default=0, help="Camera index (e.g. 0) or video file / RTSP path")
    parser.add_argument("--width", type=int, default=1280, help="Capture width")
    parser.add_argument("--height", type=int, default=720, help="Capture height")

    args = parser.parse_args()
    run_camera_stream(args.device, args.width, args.height)


if __name__ == "__main__":
    main()
