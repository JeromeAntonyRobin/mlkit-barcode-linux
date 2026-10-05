#!/usr/bin/env python3
"""
TFLite Weights Extractor
Extracts raw weight arrays, convolution kernels, biases, and embeddings
from Google ML Kit TensorFlow Lite models and exports them to NumPy (.npz) archives.
"""

import os
import sys
import glob
import numpy as np
import tflite

# Map TFLite TensorType to NumPy dtype
TFLITE_TYPE_MAP = {
    0: np.float32,
    1: np.float16,
    2: np.int32,
    3: np.uint8,
    4: np.int64,
    5: np.bytes_,
    6: np.bool_,
    7: np.int16,
    8: np.complex64,
    9: np.int8,
}


def extract_weights_from_model(tflite_path, output_npz_path):
    print("=" * 80)
    print(f"Extracting Weights: {os.path.basename(tflite_path)}")
    print("=" * 80)

    with open(tflite_path, "rb") as f:
        buf = f.read()

    model = tflite.Model.GetRootAsModel(buf, 0)
    subgraph = model.Subgraphs(0)

    weights_dict = {}
    meta_info = []

    for i in range(subgraph.TensorsLength()):
        tensor = subgraph.Tensors(i)
        buf_idx = tensor.Buffer()

        if buf_idx == 0:
            continue  # Activation tensor, not constant weight

        buffer = model.Buffers(buf_idx)
        data_len = buffer.DataLength()

        if data_len == 0:
            continue

        raw_bytes = buffer.DataAsNumpy().tobytes()
        t_name = tensor.Name().decode("utf-8") if tensor.Name() else f"tensor_{i}"
        t_type = tensor.Type()
        np_dtype = TFLITE_TYPE_MAP.get(t_type, np.uint8)
        shape = [tensor.Shape(j) for j in range(tensor.ShapeLength())]

        # Convert raw bytes to shaped numpy array
        try:
            arr = np.frombuffer(raw_bytes, dtype=np_dtype)
            if shape and np.prod(shape) == arr.size:
                arr = arr.reshape(shape)
            weights_dict[t_name] = arr
            meta_info.append({
                "name": t_name,
                "shape": shape,
                "dtype": np_dtype.__name__,
                "bytes": data_len,
            })
        except Exception as e:
            print(f"  [!] Warning on '{t_name}': {e}")

    # Save to compressed NPZ
    os.makedirs(os.path.dirname(output_npz_path), exist_ok=True)
    np.savez_compressed(output_npz_path, **weights_dict)

    total_bytes = sum(m["bytes"] for m in meta_info)
    npz_sz = os.path.getsize(output_npz_path)

    print(f"[+] Total Weight/Bias Tensors Extracted: {len(weights_dict)}")
    print(f"[+] Raw Weight Data Size:               {total_bytes:,} bytes")
    print(f"[+] Saved Compressed Archive:           {output_npz_path} ({npz_sz:,} bytes)\n")

    print("Sample Extracted Weight Tensors:")
    for item in meta_info[:8]:
        print(f"  {item['name'][:48]:48s} | Shape: {str(item['shape']):18s} | Type: {item['dtype']:7s} | {item['bytes']:,} B")
    if len(meta_info) > 8:
        print(f"  ... and {len(meta_info) - 8} more tensors.\n")

    return weights_dict


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    models_dir = os.path.join(repo_root, "models")
    weights_dir = os.path.join(models_dir, "extracted_weights")

    models = [
        ("barcode_ssd_mobilenet_v1_dmp25_quant.tflite", "ssd_mobilenet_weights.npz"),
        ("oned_feature_extractor_mobile.tflite", "oned_feature_extractor_weights.npz"),
        ("oned_auto_regressor_mobile.tflite", "oned_auto_regressor_weights.npz"),
    ]

    for model_filename, npz_filename in models:
        m_path = os.path.join(models_dir, model_filename)
        out_path = os.path.join(weights_dir, npz_filename)
        if os.path.exists(m_path):
            extract_weights_from_model(m_path, out_path)
        else:
            print(f"[-] Model file not found: {m_path}")


if __name__ == "__main__":
    main()
