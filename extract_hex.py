# converts objdump for Melee Code Manager

# Sorry, no comments for this one. Just a lot of regex

import sys
import re

def find_sequence(main_array, sequence):
    indices = []
    len_main = len(main_array)
    len_seq = len(sequence)

    if len_seq == 0:
        return list(range(len_main + 1))

    for i in range(len_main - len_seq + 1):
        if main_array[i : i + len_seq] == sequence:
            return i
    return -1

def main():
    if len(sys.argv) < 3:
        print('extract_hex.py inject.asm inject_hex.asm')
        return

    inject = {}
    inject_hex = {}

    with open(sys.argv[1], "r") as inject_file:
        inject = inject_file.read()

    with open(sys.argv[2], "r") as inject_hex_file:
        inject_hex = inject_hex_file.read()

    hex_rgx = r"(?<!\n )[0-9a-fA-F]{8}"
    hex_rgx_matches = re.findall(hex_rgx, inject_hex)
    hex_rgx_matches_orig = hex_rgx_matches

    if len(hex_rgx_matches) % 2 == 1:
        hex_rgx_matches_orig.append('00000000')

    fn_names_rgx = r"4[0-9a-fA-F]{7}\s+<(\S+)>:"
    fn_names_rgx_replacement = r"\1:"
    inject = re.sub(fn_names_rgx, fn_names_rgx_replacement, inject)

    mid_fn_jumps_rgx = r"(4[0-9a-fA-F]{7})\s+<(\S+)\+(\S+)>"
    mid_fn_jumps_rgx_replacement = r"\2_\3"
    mid_fn_jumps_matches = re.findall(mid_fn_jumps_rgx, inject)

    inject = re.sub(mid_fn_jumps_rgx, mid_fn_jumps_rgx_replacement, inject)

    labels = []

    for match in mid_fn_jumps_matches:
        label = f'{match[1]}_{match[2]}'
        if label not in labels:
            labels.append(label)
            inject = inject.replace(f'{match[0]}:', f'{label}:\n{match[0]}:')

    fn_jumps_rgx = r"4[0-9a-fA-F]{7}\s+<(\S+)>"
    fn_jumps_rgx_replacement = r"\1"
    inject = re.sub(fn_jumps_rgx, fn_jumps_rgx_replacement, inject)

    inject = inject.replace(',', ', ')

    register_addressing = r"( [\-0-9]+)(\([r0-9]+\))"
    register_addressing_matches = re.findall(register_addressing, inject)
    for match in register_addressing_matches:
        inject = inject.replace(f'{match[0]}{match[1]}', f' {hex(int(match[0]))}{match[1]}')

    pic_bcl_resolution = r"4\*cr7\+so"
    pic_bcl_resolution_replacement = r"31"
    inject = re.sub(pic_bcl_resolution, pic_bcl_resolution_replacement, inject)

    pic_crxor_resolution = r"crclr(\s+)(4\*cr1\+eq)"
    pic_crxor_resolution_replacement = r"crxor\g<1>6, 6, 6"
    inject = re.sub(pic_crxor_resolution, pic_crxor_resolution_replacement, inject)

    inject = inject.splitlines()
    checks_passed = -1

    line_chec_rgx = r"([0-9a-fA-F]{8}:\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+)(.*)"

    got2_found = False
    line_start = -1

    got_rgx = r"[0-9a-fA-F]{8}:\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+([0-9a-fA-F]{2})\s+.+"
    gor_rgx_replacement = r".long 0x\1\2\3\4"

    for line in range(len(inject)):
        if "__rei_wolf_rodata_start" in inject[line]:
            line_start = line
            break
        if "__rei_wolf_got2_start" in inject[line]:
            line_start = line
            break

    if line_start != -1:
        for line in range(line_start, len(inject)):
            inject[line] = re.sub(got_rgx, gor_rgx_replacement, inject[line])

    instructions_rgx = r"[0-9a-fA-F]{8}:\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+[0-9a-fA-F]{2}\s+(.+)"
    long_expand_rgx = r"\.long 0x([0-9a-fA-F]+)"

    for line in range(len(inject)):
        try:
            instructions_rgx_matches = re.findall(instructions_rgx, inject[line])
            inject[line] = instructions_rgx_matches[0]
        except:
            pass
        try:
            long_expand_rgx_matches = re.findall(long_expand_rgx, inject[line])
            inject[line] = f'.long 0x{f"{int(long_expand_rgx_matches[0], 16):08x}"}'
        except:
            pass

    if len(hex_rgx_matches) % 2 == 1:
        inject.append('\n_extract_hex_py_padding:')
        inject.append('.long 0x00000000')

    inject = "\n".join(inject)

    print("\n --- ASM ---")
    print(inject)
    print("\n --- BIN ---")

    second = False
    for word in hex_rgx_matches_orig:
        print(word, end='\n' if second else ' ')
        second = not second

if __name__ == '__main__':
    main()