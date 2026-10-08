import sys
import subprocess
import hashlib
import os
from pathlib import Path

def compute_nid(symbol_name):
    suffix = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
    base64s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"
    
    data = symbol_name.encode('utf-8') + suffix
    digest = hashlib.sha1(data).digest()
    
    reversed_digest = digest[7::-1]
    
    nid = ""
    for i in range(0, 6, 3):
        triple = (reversed_digest[i] << 16) | (reversed_digest[i+1] << 8) | reversed_digest[i+2]
        nid += base64s[(triple >> 18) & 0x3F]
        nid += base64s[(triple >> 12) & 0x3F]
        nid += base64s[(triple >> 6) & 0x3F]
        nid += base64s[triple & 0x3F]
        
    tail = (reversed_digest[6] << 16) | (reversed_digest[7] << 8)
    nid += base64s[(tail >> 18) & 0x3F]
    nid += base64s[(tail >> 12) & 0x3F]
    nid += base64s[(tail >> 6) & 0x3F]
    
    return nid

def main():
    if len(sys.argv) < 4:
        print("Usage: macos_nid_alias.py <output.txt> <target_name> <search_dirs...>")
        sys.exit(1)
        
    out_file = sys.argv[1]
    target_name = sys.argv[2]
    search_dirs = sys.argv[3:]
    
    obj_files = []
    # Find the target's .dir folder
    target_dir_name = f"{target_name}.dir"
    for d in search_dirs:
        for p in Path(d).rglob('*.o'):
            if target_dir_name in p.parts:
                obj_files.append(str(p))
    
    symbols = set()
    for obj in obj_files:
        try:
            out = subprocess.check_output(['nm', '-gU', obj], text=True)
            for line in out.splitlines():
                parts = line.split()
                if len(parts) >= 3 and parts[1] == 'T':
                    symbols.add(parts[2])
        except subprocess.CalledProcessError:
            pass

    with open(out_file, 'w') as f:
        for sym in symbols:
            if not sym.startswith('_'): continue
            name = sym[1:]
            if name.endswith('_nid_postfix'):
                base = name[:-12]
                nid = compute_nid(base)
                f.write(f"{sym} _{nid}\n")
            elif name.endswith('_nid_no_patch_cut'):
                base = name[:-17]
                f.write(f"{sym} _{base}\n")

if __name__ == '__main__':
    main()
