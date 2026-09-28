"""
Google ML Kit Barcode & QR Code Engine for Linux & NVIDIA Jetson
Direct native integration with libbarhopper_v3.so (no emulators, no containers)
Developed for e-con Systems Linux Vision Pipelines
"""

import ctypes
import os
import sys
import platform
import cv2
import numpy as np

# Protobuf decoder helpers
def _parse_varint(data, offset):
    res = 0
    shift = 0
    while True:
        b = data[offset]
        offset += 1
        res |= (b & 0x7f) << shift
        if not (b & 0x80):
            break
        shift += 7
    return res, offset

def _parse_protobuf(data):
    fields = []
    offset = 0
    while offset < len(data):
        tag, offset = _parse_varint(data, offset)
        field_num = tag >> 3
        wire_type = tag & 0x07
        if wire_type == 0:
            val, offset = _parse_varint(data, offset)
            fields.append((field_num, wire_type, val))
        elif wire_type == 1:
            val = data[offset:offset+8]
            offset += 8
            fields.append((field_num, wire_type, val))
        elif wire_type == 2:
            length, offset = _parse_varint(data, offset)
            val = data[offset:offset+length]
            offset += length
            fields.append((field_num, wire_type, val))
        elif wire_type == 5:
            val = data[offset:offset+4]
            offset += 4
            fields.append((field_num, wire_type, val))
        else:
            break
    return fields

def _decode_point(data):
    fields = _parse_protobuf(data)
    x = 0; y = 0
    for f_num, w_type, val in fields:
        if f_num == 1: x = val
        elif f_num == 2: y = val
    return (x, y)

def _decode_barcode(data):
    fields = _parse_protobuf(data)
    barcode = {
        "format_id": 0,
        "format": "UNKNOWN",
        "raw_bytes": b"",
        "text": "",
        "corners": []
    }
    format_names = {
        1: "CODE_128", 2: "CODE_39", 4: "CODE_93", 8: "CODABAR",
        16: "DATA_MATRIX", 32: "EAN_13", 64: "EAN_8", 128: "ITF",
        256: "QR_CODE", 512: "UPC_A", 1024: "UPC_E", 2048: "PDF417",
        4096: "AZTEC", 32768: "TEZ_CODE"
    }
    # Google Barhopper internal format mapping
    internal_to_standard = {
        1: 1, 2: 2, 3: 4, 4: 8, 5: 16, 6: 32, 7: 64, 8: 128,
        9: 256, 10: 512, 11: 1024, 12: 2048, 13: 4096, 17: 32768
    }
    for f_num, w_type, val in fields:
        if f_num == 1:
            barcode["format_id"] = internal_to_standard.get(val, val)
            barcode["format"] = format_names.get(barcode["format_id"], f"FORMAT_{val}")
        elif f_num == 2:
            barcode["raw_bytes"] = val
        elif f_num == 3:
            barcode["text"] = val.decode("utf-8", errors="replace")
        elif f_num == 11:
            barcode["corners"].append(_decode_point(val))
    return barcode

class _MockBitmap(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
        ("stride", ctypes.c_uint32),
        ("format", ctypes.c_int32),
        ("pixels", ctypes.c_void_p)
    ]

class MLKitBarcodeScanner:
    """
    High-performance native Google ML Kit Barcode Scanner for Linux.
    Runs libbarhopper_v3.so directly using a lightweight Bionic shim.
    """
    def __init__(self, repo_root=None):
        if repo_root is None:
            # Auto-locate based on file location: sdk/python/../../
            current_dir = os.path.dirname(os.path.abspath(__file__))
            self.repo_root = os.path.abspath(os.path.join(current_dir, "..", ".."))
        else:
            self.repo_root = os.path.abspath(repo_root)

        arch = platform.machine().lower()
        if arch in ["aarch64", "arm64"]:
            arch_dir = "arm64-v8a"
        else:
            arch_dir = "x86_64"

        shim_so = os.path.join(self.repo_root, "core", "lib", arch_dir, "libandroid_shim.so")
        barhopper_so = os.path.join(self.repo_root, "core", "lib", arch_dir, "libbarhopper_v3_patched.so")
        opts_bin = os.path.join(self.repo_root, "models", "barhopper_options_official.bin")

        if not os.path.exists(shim_so):
            raise FileNotFoundError(f"Android shim library not found at: {shim_so}")
        if not os.path.exists(barhopper_so):
            raise FileNotFoundError(f"Patched Barhopper library not found at: {barhopper_so}")
        if not os.path.exists(opts_bin):
            raise FileNotFoundError(f"Barhopper options configuration not found at: {opts_bin}")

        self.shim = ctypes.CDLL(shim_so, mode=ctypes.RTLD_GLOBAL)
        self.barhopper = ctypes.CDLL(barhopper_so, mode=ctypes.RTLD_GLOBAL)

        # Redirect Barhopper internal stdio to fd 2
        self.shim.HookBarhopperStdio.argtypes = [ctypes.c_void_p]
        self.shim.HookBarhopperStdio(self.barhopper._handle)

        # Bind JNI helper functions
        self.shim.GetMockJNIEnv.restype = ctypes.c_void_p
        self.shim.CreateMockByteArrayWithData.restype = ctypes.c_void_p
        self.shim.CreateMockByteArrayWithData.argtypes = [ctypes.c_void_p, ctypes.c_int32]
        self.shim.FreeMockByteArray.argtypes = [ctypes.c_void_p]

        self.shim.GetLastProtobufResponse.restype = ctypes.c_void_p
        self.shim.GetLastProtobufResponse.argtypes = [ctypes.POINTER(ctypes.c_int32)]

        # Barhopper exports
        self.create_fn = self.barhopper.Java_com_google_android_libraries_barhopper_BarhopperV3_createNativeWithClientOptions
        self.create_fn.restype = ctypes.c_int64
        self.create_fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

        self.close_fn = self.barhopper.Java_com_google_android_libraries_barhopper_BarhopperV3_closeNative
        self.close_fn.restype = None
        self.close_fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int64]

        self.rec_bitmap_fn = self.barhopper.Java_com_google_android_libraries_barhopper_BarhopperV3_recognizeBitmapNative
        self.rec_bitmap_fn.restype = ctypes.c_void_p
        self.rec_bitmap_fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int64, ctypes.c_void_p, ctypes.c_void_p]

        self.env = self.shim.GetMockJNIEnv()
        self.options_ptr = ctypes.c_void_p(0x100)

        # Initialize engine with protobuf options & TFLite model weights
        with open(opts_bin, "rb") as f:
            proto_bytes = f.read()
        arr_ptr = self.shim.CreateMockByteArrayWithData(proto_bytes, len(proto_bytes))
        self.context = self.create_fn(self.env, None, arr_ptr)
        self.shim.FreeMockByteArray(arr_ptr)
        self.out_len = ctypes.c_int32(0)

    def scan(self, frame_bgr):
        """
        Scan a BGR OpenCV image and return list of detected barcodes.
        Each barcode is a dict with format, text, corners, raw_bytes.
        """
        img_rgba = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGBA)
        h, w, _ = img_rgba.shape
        rgba_bytes = img_rgba.tobytes()

        bm = _MockBitmap()
        bm.width = w
        bm.height = h
        bm.stride = w * 4
        bm.format = 1 # RGBA_8888
        bm.pixels = ctypes.cast(ctypes.c_char_p(rgba_bytes), ctypes.c_void_p)

        self.rec_bitmap_fn(self.env, None, self.context, ctypes.byref(bm), self.options_ptr)
        proto_ptr = self.shim.GetLastProtobufResponse(ctypes.byref(self.out_len))

        barcodes = []
        if self.out_len.value > 0:
            data = ctypes.string_at(proto_ptr, self.out_len.value)
            fields = _parse_protobuf(data)
            for f_num, w_type, val in fields:
                if f_num == 1:
                    barcodes.append(_decode_barcode(val))
        return barcodes

    def close(self):
        if self.context:
            self.close_fn(self.env, None, self.context)
            self.context = None

# Backward compatibility alias
GoogleMLKitBarcodeScanner = MLKitBarcodeScanner

if __name__ == "__main__":
    scanner = MLKitBarcodeScanner()
    sample_files = [
        os.path.join(scanner.repo_root, "samples", f)
        for f in ["sample_qr.png", "sample_code128.png", "sample_qr_rotated.png"]
    ]
    for sample in sample_files:
        if not os.path.exists(sample):
            continue
        frame = cv2.imread(sample)
        results = scanner.scan(frame)
        print(f"\n[*] Results for {os.path.basename(sample)}:")
        for b in results:
            print(f"    Format:  {b['format']}")
            print(f"    Text:    {b['text']}")
            print(f"    Corners: {b['corners']}")
    scanner.close()
    os._exit(0)
