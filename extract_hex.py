#!/usr/bin/env python3

# Converts objdump output for Melee Code Manager

import sys
import re
from pathlib import Path

def read_base_address_from_linker(linker_path):
    # extract base address from linker script
    try:
        with open(linker_path, 'r') as f:
            content = f.read()
            match = re.search(r'\.\s*=\s*(0x[0-9A-Fa-f]+)\s*;', content)
            if match:
                addr = int(match.group(1), 16)
                return addr
    except:
        return None

def main():
    if len(sys.argv) < 3:
        print('Usage: extract_hex.py <inject.asm> <inject_hex.asm> [linker_script.ld]')
        print('Example: extract_hex.py build/output/inject.asm build/output/inject_hex.asm build/src/melee.ld')
        return

    asm_file = sys.argv[1]
    hex_file = sys.argv[2]
    linker_file = sys.argv[3] if len(sys.argv) > 3 else 'build/src/melee.ld'

    base_addr = read_base_address_from_linker(linker_file)
    
    if base_addr:
        addr_prefix = f"{(base_addr >> 24) & 0xFF:x}"
        print(f"Looking for addresses starting with: 0x{addr_prefix}xxxxxx")
    else:
        print("Warning: Using fallback address detection (4xxxxxxx pattern)")
        addr_prefix = '4'

    with open(asm_file, "r") as f:
        inject = f.read()

    with open(hex_file, "r") as f:
        inject_hex = f.read()

    hex_rgx = r"(?<!\n )[0-9a-fA-F]{8}"
    hex_rgx_matches = re.findall(hex_rgx, inject_hex)
    hex_rgx_matches_orig = hex_rgx_matches.copy()

    if len(hex_rgx_matches) % 2 == 1:
        hex_rgx_matches_orig.append('00000000')

    header_pattern = r'^.*?file format.*?$'
    inject = re.sub(header_pattern, '', inject, flags=re.MULTILINE)
    
    disasm_pattern = r'^Disassembly of section .*?$'
    inject = re.sub(disasm_pattern, '', inject, flags=re.MULTILINE)
    
    addr_to_label = {}
    function_names = set()
    mid_function_labels = set()
    
    if base_addr:
        fn_header_pattern = rf"({addr_prefix}[0-9a-fA-F]{{6}})\s+<([^>]+)>:"
    else:
        fn_header_pattern = r"(4[0-9a-fA-F]{7})\s+<([^>]+)>:"
    
    print(f"Function header pattern: {fn_header_pattern}")
    
    for match in re.finditer(fn_header_pattern, inject):
        addr = match.group(1)
        name = match.group(2)
        addr_to_label[addr] = name
        function_names.add(name)
        print(f"  Function: {addr} -> {name}")
    
    if base_addr:
        mid_fn_pattern = rf"({addr_prefix}[0-9a-fA-F]{{6}})\s+<([^>]+)\+([^>]+)>"
    else:
        mid_fn_pattern = r"(4[0-9a-fA-F]{7})\s+<([^>]+)\+([^>]+)>"
    
    for match in re.finditer(mid_fn_pattern, inject):
        addr = match.group(1)
        func = match.group(2)
        offset = match.group(3)
        label = f"{func}_{offset}"
        addr_to_label[addr] = label
        mid_function_labels.add(label)
        print(f"  Mid-func: {addr} -> {label}")
    
    print(f"\nTotal labels: {len(addr_to_label)}")
    print(f"Functions: {len(function_names)}")
    print(f"Mid-function: {len(mid_function_labels)}\n")

    lines = inject.split('\n')
    processed_lines = []
    in_data_section = False
    prev_was_label = False
    
    for line in lines:
        stripped = line.strip()
        
        if not stripped:
            continue
        
        if "__rei_wolf_rodata_lo" in stripped or "__rei_wolf_got2_lo" in stripped or "_patch_inject_init_va" in stripped or "INJECT_Regions" in stripped or "inject_selection" in stripped or "__rei_wolf_data" in stripped:
            in_data_section = True
        
        if base_addr:
            line_addr_pattern = rf"^({addr_prefix}[0-9a-fA-F]{{6}}):"
        else:
            line_addr_pattern = r"^(4[0-9a-fA-F]{7}):"
        
        line_addr_match = re.match(line_addr_pattern, stripped)
        
        if base_addr:
            header_pattern = rf"^{addr_prefix}[0-9a-fA-F]{{7}}\s+<[^>]+>:"
        else:
            header_pattern = r"^4[0-9a-fA-F]{7}\s+<[^>]+>:"
        
        if re.match(header_pattern, stripped):
            label_match = re.search(r"<([^>]+)>:", stripped)
            if label_match:
                label_name = label_match.group(1)
                if label_name not in mid_function_labels and processed_lines and not prev_was_label:
                    processed_lines.append('')
                processed_lines.append(f"{label_name}:")
                prev_was_label = True
            continue
        
        if line_addr_match:
            addr = line_addr_match.group(1)
            
            if in_data_section:
                if addr in addr_to_label and addr_to_label[addr] in function_names:
                    label = addr_to_label[addr]
                    if processed_lines and not prev_was_label:
                        processed_lines.append('')
                    processed_lines.append(f"{label}:")
                    prev_was_label = True
            else:
                if addr in addr_to_label:
                    label = addr_to_label[addr]
                    if label not in mid_function_labels and processed_lines and not prev_was_label:
                        processed_lines.append('')
                    processed_lines.append(f"{label}:")
                    prev_was_label = True
            
            if in_data_section:
                data_match = re.match(
                    r"[0-9a-fA-F]{8}:\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+.*",
                    stripped
                )
                if data_match:
                    hex_val = f"{data_match.group(1)}{data_match.group(2)}{data_match.group(3)}{data_match.group(4)}"
                    processed_lines.append(f".long 0x{hex_val}")
                    prev_was_label = False
                    continue
            else:
                instr_match = re.match(
                    r"[0-9a-fA-F]{8}:\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+(.+)",
                    stripped
                )
                if instr_match:
                    instruction = instr_match.group(1)
                    
                    def replace_addr_in_instr(match):
                        addr = match.group(1)
                        return addr_to_label.get(addr, addr)
                    
                    if base_addr:
                        addr_ref_pattern = rf"({addr_prefix}[0-9a-fA-F]{{6}})\s+<[^>]+>"
                    else:
                        addr_ref_pattern = r"(4[0-9a-fA-F]{7})\s+<[^>]+>"
                    
                    instruction = re.sub(addr_ref_pattern, replace_addr_in_instr, instruction)
                    
                    instruction = instruction.replace('4*cr7+so', '31')
                    instruction = re.sub(r'crclr\s+4\*cr1\+eq', r'crxor   6, 6, 6', instruction)
                    
                    if 'crxor' not in instruction:
                        instruction = instruction.replace(',', ', ')
                    
                    offset_matches = re.findall(r' ([\-0-9]+)(\([r0-9]+\))', instruction)
                    for offset_str, reg in offset_matches:
                        instruction = instruction.replace(f' {offset_str}{reg}', f' {hex(int(offset_str))}{reg}')
                    
                    processed_lines.append(instruction)
                    prev_was_label = False
                    continue
        
        long_match = re.match(r'\.long 0x([0-9a-fA-F]+)', stripped)
        if long_match:
            hex_val = long_match.group(1)
            processed_lines.append(f'.long 0x{int(hex_val, 16):08x}')
            prev_was_label = False
            continue

    if len(hex_rgx_matches) % 2 == 1:
        processed_lines.append('')
        processed_lines.append('_extract_hex_py_padding:')
        processed_lines.append('.long 0x00000000')

    inject = "\n".join(processed_lines)

    print("============================================================")
    print(" ASM OUTPUT")
    print("============================================================")
    print(inject)
    
    print("============================================================")
    print(" BINARY OUTPUT")
    print("============================================================")

    second = False
    for word in hex_rgx_matches_orig:
        print(word, end='\n' if second else ' ')
        second = not second
    
    print()
    
    byte_count = len(hex_rgx_matches_orig) * 4
    word_count = len(hex_rgx_matches_orig)
    
    print("\n============================================================")
    print(f"Bytes:      {byte_count} (0x{byte_count:X})")
    print(f"Words:      {word_count} (0x{word_count:X})")
    print(f"Code size:  {byte_count / 1024:.2f} KB")    
    print("============================================================")

if __name__ == '__main__':
    main()