import struct
import shutil

src = "core/lib/arm64-v8a/libbarhopper_v3_patched.so"
backup = "/tmp/libbarhopper_v3_patched.so.bak"
shutil.copyfile(src, backup)

with open(src, "r+b") as f:
    f.seek(0x4ad118)
    for i in range(32):
        pos = f.tell()
        tag, val = struct.unpack("<QQ", f.read(16))
        # If tag is DT_VERDEF (0x6ffffffc), DT_VERDEFNUM (0x6ffffffd),
        # or DT_DEBUG (0x15) in the version area [26..30]
        if i in (26, 27, 28, 29, 30):
            print(f"Neutralizing entry {i}: tag 0x{tag:x}, val 0x{val:x} -> tag 0x15, val 0")
            f.seek(pos)
            f.write(struct.pack("<QQ", 0x15, 0))

print("[+] Successfully neutralized version tags in ARM64 binary.")
