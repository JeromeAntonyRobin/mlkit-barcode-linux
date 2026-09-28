#!/usr/bin/env python3
"""
Google ML Kit Barcode & QR Code Engine for Linux & NVIDIA Jetson
Main CLI entry point
"""
import os
import sys
import cv2

# Ensure repo root is in python path
REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from sdk.python.mlkit_scanner import MLKitBarcodeScanner, GoogleMLKitBarcodeScanner

def main():
    scanner = MLKitBarcodeScanner()
    
    samples = [
        os.path.join(REPO_ROOT, "samples", f)
        for f in ["sample_qr.png", "sample_code128.png", "sample_qr_rotated.png", "sample_qr_blurred.png"]
    ]
    
    print("\n" + "="*60)
    print(" Google ML Kit Barcode Scanner - Linux / Jetson Orin Nano ")
    print("="*60)
    
    for sample in samples:
        if not os.path.exists(sample):
            continue
        frame = cv2.imread(sample)
        results = scanner.scan(frame)
        print(f"\n[*] Sample: {os.path.basename(sample)}")
        if not results:
            print("    [!] No barcode detected")
        for b in results:
            print(f"    Format:  {b['format']} (ID: {b['format_id']})")
            print(f"    Text:    {b['text']}")
            print(f"    Corners: {b['corners']}")
            
    scanner.close()
    print("\n" + "="*60)
    os._exit(0)

if __name__ == "__main__":
    main()
