import struct
import math

# Google Barhopper V3 Options Generator
# Derived directly from com.google.mlkit.vision.barcode.bundled.internal.zza bytecode

def encode_varint(value):
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    out.append(value & 0x7F)
    return bytes(out)

def encode_field_tag(field_num, wire_type):
    return encode_varint((field_num << 3) | wire_type)

def encode_length_delimited(field_num, data):
    tag = encode_field_tag(field_num, 2)
    return tag + encode_varint(len(data)) + data

def encode_varint_field(field_num, value):
    return encode_field_tag(field_num, 0) + encode_varint(value)

def encode_fixed32_field(field_num, float_val):
    tag = encode_field_tag(field_num, 5)
    return tag + struct.pack("<f", float_val)

def generate_anchor_layers():
    zza = [5, 7, 7, 7, 5, 5]
    zzb = [
        [0.075, 1.0], [0.1, 1.0], [0.125, 1.0], [0.2, 2.0], [0.2, 0.5],
        [0.15, 1.0], [0.2, 1.0], [0.25, 1.0], [0.35, 2.0], [0.35, 0.5], [0.35, 3.0], [0.35, 0.3333],
        [0.3, 1.0], [0.4, 1.0], [0.5, 1.0], [0.5, 2.0], [0.5, 0.5], [0.5, 3.0], [0.5, 0.3333],
        [0.6, 1.0], [0.8, 1.0], [1.0, 1.0], [0.65, 2.0], [0.65, 0.5], [0.65, 3.0], [0.65, 0.3333],
        [1.0, 1.0], [0.8, 2.0], [0.8, 0.5], [0.8, 3.0], [0.8, 0.3333],
        [1.0, 1.0], [0.95, 2.0], [0.95, 0.5], [0.95, 3.0], [0.95, 0.3333]
    ]

    stride = 16
    zzb_idx = 0
    anchor_layers_proto = bytearray()

    for layer_idx in range(6):
        layer_buf = bytearray()
        # field 3: stride_x = stride
        layer_buf += encode_varint_field(3, stride)
        # field 4: stride_y = stride
        layer_buf += encode_varint_field(4, stride)

        num_anchors = zza[layer_idx]
        heights = []
        widths = []
        for _ in range(num_anchors):
            scale, aspect_ratio = zzb[zzb_idx]
            zzb_idx += 1
            scale_320 = scale * 320.0
            sqrt_ar = math.sqrt(aspect_ratio)
            h = scale_320 / sqrt_ar
            w = scale_320 * sqrt_ar
            heights.append(h)
            widths.append(w)

        # field 1: repeated float heights (packed or unpacked? Google protobuf generated: field 1 tag 0x13 -> wire type 2 packed or wire type 5)
        # In proto3, repeated float is packed by default (wire type 2)
        # Packed field 1:
        packed_heights = struct.pack(f"<{len(heights)}f", *heights)
        layer_buf += encode_length_delimited(1, packed_heights)

        # Packed field 2:
        packed_widths = struct.pack(f"<{len(widths)}f", *widths)
        layer_buf += encode_length_delimited(2, packed_widths)

        # AnchorLayers (zzf) has repeated zzc field 1
        anchor_layers_proto += encode_length_delimited(1, bytes(layer_buf))
        stride *= 2

    return bytes(anchor_layers_proto)

def build_barhopper_v3_options():
    # Load model bytes
    model_ssd = open("approach3_android_aar_extract/extracted_aar/assets/mlkit_barcode_models/barcode_ssd_mobilenet_v1_dmp25_quant.tflite", "rb").read()
    model_feat = open("approach3_android_aar_extract/extracted_aar/assets/mlkit_barcode_models/oned_feature_extractor_mobile.tflite", "rb").read()
    model_reg = open("approach3_android_aar_extract/extracted_aar/assets/mlkit_barcode_models/oned_auto_regressor_mobile.tflite", "rb").read()

    # 1. BarcodeDetectorClientOptions (zzi)
    # field 2: bytes ssd_model
    # field 6: AnchorLayers zzj
    anchor_layers_bytes = generate_anchor_layers()
    detector_opts = bytearray()
    detector_opts += encode_length_delimited(2, model_ssd)
    detector_opts += encode_length_delimited(6, anchor_layers_bytes)

    # 2. OnedDecoderClientOptions (zzac)
    # Stream 710: oned_auto_regressor_mobile.tflite -> zzab.zza -> zzac.zzh (field 4)
    # Stream 712: oned_feature_extractor_mobile.tflite -> zzab.zzb -> zzac.zzf (field 2)
    oned_opts = bytearray()
    oned_opts += encode_length_delimited(2, model_feat)
    oned_opts += encode_length_delimited(4, model_reg)

    # 3. BarhopperV3Options
    # field 1: BarcodeDetectorClientOptions
    # field 2: OnedDecoderClientOptions
    v3_opts = bytearray()
    v3_opts += encode_length_delimited(1, bytes(detector_opts))
    v3_opts += encode_length_delimited(2, bytes(oned_opts))

    return bytes(v3_opts)

if __name__ == "__main__":
    opts_bytes = build_barhopper_v3_options()
    out_path = "approach3_android_aar_extract/native_shim/barhopper_options_official.bin"
    with open(out_path, "wb") as f:
        f.write(opts_bytes)
    print(f"Generated official BarhopperV3Options protobuf: {len(opts_bytes)} bytes written to {out_path}")
