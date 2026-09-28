import struct

def parse_varint(data, offset):
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

def parse_protobuf(data):
    fields = []
    offset = 0
    while offset < len(data):
        tag, offset = parse_varint(data, offset)
        field_num = tag >> 3
        wire_type = tag & 0x07
        if wire_type == 0: # Varint
            val, offset = parse_varint(data, offset)
            fields.append((field_num, wire_type, val))
        elif wire_type == 1: # 64-bit
            val = data[offset:offset+8]
            offset += 8
            fields.append((field_num, wire_type, val))
        elif wire_type == 2: # Length-delimited
            length, offset = parse_varint(data, offset)
            val = data[offset:offset+length]
            offset += length
            fields.append((field_num, wire_type, val))
        elif wire_type == 5: # 32-bit
            val = data[offset:offset+4]
            offset += 4
            fields.append((field_num, wire_type, val))
        else:
            raise ValueError(f"Unsupported wire type {wire_type} at offset {offset}")
    return fields

def decode_point(data):
    fields = parse_protobuf(data)
    x = 0
    y = 0
    for f_num, w_type, val in fields:
        if f_num == 1:
            x = val
        elif f_num == 2:
            y = val
    return (x, y)

def decode_barcode(data):
    fields = parse_protobuf(data)
    barcode = {
        "format": 0,
        "raw_bytes": b"",
        "display_value": "",
        "corner_points": []
    }
    for f_num, w_type, val in fields:
        if f_num == 1:
            barcode["format"] = val
        elif f_num == 2:
            barcode["raw_bytes"] = val
        elif f_num == 3:
            barcode["display_value"] = val.decode("utf-8", errors="replace")
        elif f_num == 11: # zzt (corner points)
            pt = decode_point(val)
            barcode["corner_points"].append(pt)
    return barcode

def decode_barhopper_response(data):
    fields = parse_protobuf(data)
    response = {
        "status": 0,
        "barcodes": []
    }
    for f_num, w_type, val in fields:
        if f_num == 1: # repeated Barcode zzc
            bc = decode_barcode(val)
            response["barcodes"].append(bc)
        elif f_num == 2: # status zzf
            response["status"] = val
    return response

if __name__ == "__main__":
    import sys
    test_raw = bytes.fromhex("1000")
    print("Test parse 1000:", decode_barhopper_response(test_raw))
