import struct

src = "approach3_android_aar_extract/extracted_aar/jni/arm64-v8a/libbarhopper_v3.so"
dst = "approach3_android_aar_extract/extracted_aar/jni/arm64-v8a/libbarhopper_v3_patched.so"

with open(src, "rb") as f:
    data = bytearray(f.read())

# Read ELF 64 header
magic = data[:4]
assert magic == b"\x7fELF", "Not an ELF file"

# Section header table offset
e_shoff = struct.unpack("<Q", data[40:48])[0]
e_shentsize = struct.unpack("<H", data[58:60])[0]
e_shnum = struct.unpack("<H", data[60:62])[0]
e_shstrndx = struct.unpack("<H", data[62:64])[0]

# Program header table
e_phoff = struct.unpack("<Q", data[32:40])[0]
e_phentsize = struct.unpack("<H", data[54:56])[0]
e_phnum = struct.unpack("<H", data[56:58])[0]

# Locate PT_DYNAMIC (type 2)
dynamic_offset = None
dynamic_size = None
for i in range(e_phnum):
    ph = data[e_phoff + i * e_phentsize : e_phoff + (i + 1) * e_phentsize]
    p_type = struct.unpack("<I", ph[:4])[0]
    if p_type == 2: # PT_DYNAMIC
        dynamic_offset = struct.unpack("<Q", ph[8:16])[0]
        dynamic_size = struct.unpack("<Q", ph[32:40])[0]
        break

print(f"[*] PT_DYNAMIC found at offset 0x{dynamic_offset:x}, size {dynamic_size} bytes")

DT_DEBUG = 0x15
DT_NEEDED = 0x1
DT_VERSYM = 0x6ffffff0
DT_VERNEED = 0x6ffffffe
DT_VERNEEDNUM = 0x6fffffff

# Neutralize DT_NEEDED and DT_VER* tags in dynamic section
num_entries = dynamic_size // 16
for i in range(num_entries):
    entry_off = dynamic_offset + i * 16
    d_tag = struct.unpack("<Q", data[entry_off : entry_off + 8])[0]
    if d_tag in (DT_NEEDED, DT_VERSYM, DT_VERNEED, DT_VERNEEDNUM):
        print(f"  -> Converting d_tag 0x{d_tag:x} to DT_DEBUG (0x15) at offset 0x{entry_off:x}")
        data[entry_off : entry_off + 8] = struct.pack("<Q", DT_DEBUG)

with open(dst, "wb") as f:
    f.write(data)

print(f"[+] Successfully patched ARM64 binary: {dst}")
