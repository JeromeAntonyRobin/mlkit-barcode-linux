#!/usr/bin/env python3
"""
ELF Dynamic Section Patching Utility for Android Bionic on Linux
Neutralizes DT_NEEDED, DT_VERSYM, DT_VERDEF, DT_VERDEFNUM, and DT_VERNEED tags
so that standard Glibc dynamic linkers (ld.so) load Android Bionic shared libraries
without throwing version conflicts or segmentation faults.
"""

import sys
import os
import struct
import shutil
import argparse


DT_NULL = 0x0
DT_NEEDED = 0x1
DT_DEBUG = 0x15
DT_VERSYM = 0x6ffffff0
DT_VERDEF = 0x6ffffffc
DT_VERDEFNUM = 0x6ffffffd
DT_VERNEED = 0x6ffffffe
DT_VERNEEDNUM = 0x6fffffff

TARGET_TAGS = {
    DT_NEEDED: "DT_NEEDED",
    DT_VERSYM: "DT_VERSYM",
    DT_VERDEF: "DT_VERDEF",
    DT_VERDEFNUM: "DT_VERDEFNUM",
    DT_VERNEED: "DT_VERNEED",
    DT_VERNEEDNUM: "DT_VERNEEDNUM",
}


def patch_elf_dynamic(src_path, dst_path=None, neutralize_needed=False):
    if dst_path is None:
        dst_path = src_path

    if src_path != dst_path:
        shutil.copyfile(src_path, dst_path)

    with open(dst_path, "r+b") as f:
        data = f.read()

        if data[:4] != b"\x7fELF":
            print(f"[-] Error: '{src_path}' is not an ELF binary.")
            return False

        ei_class = data[4]
        if ei_class != 2:
            print("[-] Error: Only 64-bit ELF (ELFCLASS64) binaries are supported.")
            return False

        endian = "<" if data[5] == 1 else ">"

        # Read Program Header table
        e_phoff = struct.unpack_from(endian + "Q", data, 0x20)[0]
        e_phentsize = struct.unpack_from(endian + "H", data, 0x36)[0]
        e_phnum = struct.unpack_from(endian + "H", data, 0x38)[0]

        # Locate PT_DYNAMIC segment (type 2)
        dyn_offset = None
        dyn_filesz = None
        for i in range(e_phnum):
            ph_off = e_phoff + i * e_phentsize
            p_type = struct.unpack_from(endian + "I", data, ph_off)[0]
            if p_type == 2:  # PT_DYNAMIC
                dyn_offset = struct.unpack_from(endian + "Q", data, ph_off + 8)[0]
                dyn_filesz = struct.unpack_from(endian + "Q", data, ph_off + 32)[0]
                break

        if dyn_offset is None:
            print("[-] Error: PT_DYNAMIC segment not found.")
            return False

        print(f"[*] Found PT_DYNAMIC at offset 0x{dyn_offset:x} (size: {dyn_filesz:,} bytes)")

        num_entries = dyn_filesz // 16
        patched_count = 0

        for i in range(num_entries):
            entry_off = dyn_offset + i * 16
            tag, val = struct.unpack_from(endian + "QQ", data, entry_off)

            if tag == DT_NULL:
                break

            # Tags to neutralize
            should_patch = False
            if tag in (DT_VERSYM, DT_VERDEF, DT_VERDEFNUM, DT_VERNEED, DT_VERNEEDNUM):
                should_patch = True
            elif neutralize_needed and tag == DT_NEEDED:
                should_patch = True

            if should_patch:
                tag_name = TARGET_TAGS.get(tag, f"0x{tag:x}")
                print(f"  [+] Neutralizing {tag_name} (tag=0x{tag:x}, val=0x{val:x}) -> DT_DEBUG (0x15, val 0)")
                f.seek(entry_off)
                f.write(struct.pack(endian + "QQ", DT_DEBUG, 0))
                patched_count += 1

        print(f"[+] Successfully neutralized {patched_count} dynamic tag(s) in '{dst_path}'")
        return True


def main():
    parser = argparse.ArgumentParser(description="Patch ELF dynamic section tags for Android-to-Linux compatibility")
    parser.add_argument("input", help="Path to input .so shared library")
    parser.add_argument("output", nargs="?", default=None, help="Path to output patched library (defaults to in-place)")
    parser.add_argument("--neutralize-needed", action="store_true", help="Also neutralize DT_NEEDED dependencies")

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"[-] Error: input file not found: {args.input}")
        sys.exit(1)

    ok = patch_elf_dynamic(args.input, args.output, args.neutralize_needed)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
