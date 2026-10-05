#!/usr/bin/env python3
"""
Google ML Kit Neural Network Model Inspector & Standalone Inference Runner
Inspects embedded TFLite models and executes pure-Python inference using Google's LiteRT runtime.
"""

import os
import sys
import glob
import cv2
import numpy as np

# Ensure venv packages can be used
try:
    from ai_edge_litert.interpreter import Interpreter
except ImportError:
    try:
        from tensorflow.lite.python.interpreter import Interpreter
    except ImportError:
        print("[-] Error: 'ai-edge-litert' or 'tensorflow' runtime not found.")
        print("    Run with: .venv/bin/python3 core/tools/tflite_model_inspector.py")
        sys.exit(1)


def inspect_all_models(models_dir):
    print("=" * 80)
    print(" GOOGLE ML KIT - EMBEDDED TENSORFLOW LITE MODEL INSPECTOR ")
    print("=" * 80)

    model_files = sorted(glob.glob(os.path.join(models_dir, "*.tflite")))
    if not model_files:
        print(f"[-] No .tflite models found in {models_dir}")
        return

    for path in model_files:
        name = os.path.basename(path)
        sz = os.path.getsize(path)
        print(f"\n[+] Model: {name} ({sz:,} bytes)")
        print("-" * 80)

        interp = Interpreter(model_path=path)
        interp.allocate_tensors()

        inputs = interp.get_input_details()
        outputs = interp.get_output_details()
        tensors = interp.get_tensor_details()

        print("  Inputs:")
        for idx, inp in enumerate(inputs):
            q_scale, q_zero = inp.get("quantization", (0.0, 0))
            q_str = f" | Scale={q_scale:.6f}, ZeroPoint={q_zero}" if q_scale != 0.0 else ""
            print(f"    [{idx}] {inp['name']}")
            print(f"        Shape: {inp['shape'].tolist()} | Type: {inp['dtype'].__name__}{q_str}")

        print("  Outputs:")
        for idx, out in enumerate(outputs):
            q_scale, q_zero = out.get("quantization", (0.0, 0))
            q_str = f" | Scale={q_scale:.6f}, ZeroPoint={q_zero}" if q_scale != 0.0 else ""
            print(f"    [{idx}] {out['name']}")
            print(f"        Shape: {out['shape'].tolist()} | Type: {out['dtype'].__name__}{q_str}")

        print(f"  Total Internal Tensors: {len(tensors)}")


def run_ssd_detection_demo(ssd_model_path, sample_img_path):
    print("\n" + "=" * 80)
    print(" STANDALONE SSD DETECTOR INFERENCE DEMO (LiteRT / TFLite) ")
    print("=" * 80)

    if not os.path.exists(ssd_model_path):
        print(f"[-] SSD model not found: {ssd_model_path}")
        return
    if not os.path.exists(sample_img_path):
        print(f"[-] Sample image not found: {sample_img_path}")
        return

    print(f"[*] Loading model: {os.path.basename(ssd_model_path)}")
    interp = Interpreter(model_path=ssd_model_path)
    interp.allocate_tensors()

    # Preprocess image according to model input contract: 1 x 320 x 320 x 1 (uint8)
    img = cv2.imread(sample_img_path, cv2.IMREAD_GRAYSCALE)
    orig_h, orig_w = img.shape
    resized = cv2.resize(img, (320, 320))
    input_tensor = np.expand_dims(resized, axis=(0, 3)).astype(np.uint8)

    inp_info = interp.get_input_details()[0]
    interp.set_tensor(inp_info["index"], input_tensor)

    print(f"[*] Running forward pass on '{os.path.basename(sample_img_path)}' ({orig_w}x{orig_h} -> 320x320)...")
    interp.invoke()

    print("[+] Inference completed successfully!\n")
    print("  Feature Map Prediction Heads:")
    print("  " + "-" * 65)

    outputs = interp.get_output_details()
    # Sort outputs by BoxPredictor level
    outputs_sorted = sorted(outputs, key=lambda x: x["name"])

    for out in outputs_sorted:
        data = interp.get_tensor(out["index"])
        name = out["name"]
        head_name = name.split("/")[0]
        pred_type = "Box Regressor" if "BoxEncodingPredictor" in name else "Class Confidence"
        scale, zp = out.get("quantization", (0.0, 0))

        # Find maximum activation in the feature map
        max_val = data.max()
        dequant_max = (float(max_val) - zp) * scale if scale != 0.0 else max_val

        print(f"  {head_name:16s} | {pred_type:17s} | Shape: {str(list(data.shape)):14s} | Max Act: {max_val:3d} ({dequant_max:+.2f})")

    print("\n[+] The SSD MobileNet model extracts candidate barcode anchors across 6 pyramid scales.")


def print_mlkit_pipeline_architecture():
    print("\n" + "=" * 80)
    print(" GOOGLE ML KIT BARCODE ENGINE - COMPLETE NEURAL PIPELINE ")
    print("=" * 80)
    pipeline_diagram = """
    Input Image (Camera / Bitmap: BGR / NV21 / RGBA)
           │
           ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  STAGE 1: 2D Spatial Localization (SSD MobileNet V1 DMP25)  │
    │  - Input:  320 x 320 x 1 Grayscale (uint8 quantized)        │
    │  - Heads:  6 Multi-Scale Feature Maps (20x20 down to 1x1)   │
    │  - Output: 2D Bounding Boxes & Confidence Anchors           │
    └──────────────────────────────┬──────────────────────────────┘
                                   │
                    Candidate Trajectory Crop
                                   │
                                   ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  STAGE 2: 1D Trajectory Feature Extractor (CNN)             │
    │  - Input:  32 x 704 x 1 Oriented Strip (uint8 quantized)    │
    │  - Output: 22 x 128 Spatial Feature Embeddings              │
    └──────────────────────────────┬──────────────────────────────┘
                                   │
                     Sequence Feature Vectors
                                   │
                                   ▼
    ┌─────────────────────────────────────────────────────────────┐
    │  STAGE 3: 1D Autoregressive Sequence Decoder (RNN/Attention)│
    │  - Input:  22 x 128 Visual Features + Target Token History  │
    │  - Output: 242-Class Logits + Direction Flag + Is-Barcode   │
    │  - Decodes full barcode text character-by-character         │
    └─────────────────────────────────────────────────────────────┘
    """
    print(pipeline_diagram)


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    models_dir = os.path.join(repo_root, "models")
    ssd_model = os.path.join(models_dir, "barcode_ssd_mobilenet_v1_dmp25_quant.tflite")
    sample_img = os.path.join(repo_root, "samples", "sample_qr.png")

    inspect_all_models(models_dir)
    print_mlkit_pipeline_architecture()
    run_ssd_detection_demo(ssd_model, sample_img)


if __name__ == "__main__":
    main()
