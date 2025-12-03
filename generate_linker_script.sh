#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input-melee.link> <output-linker.ld>" >&2
    exit 1
fi

IN="$1"
OUT="$2"

# I have no idea what I'm doing, but this seems
# to be a working, albeit nuclear, approach.

{
    echo "/* SSBM symbols */"
    echo ""

    sed -E 's/0?x?([0-9A-Fa-f]+):([^$]+)$/\2 = 0x\1;/g' "$IN"

    echo ""
    cat <<'EOF'
SECTIONS
{
    . = 0x40000000;

    .text : {
        *(.text.__patch)
        *(.text)
        *(.text.*)
        . = ALIGN(4);
        __rei_wolf_rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        __rei_wolf_got2_start = .;
        *(.got2)
        . = ALIGN(4);
        __rei_wolf_got2_end = .;
        __rei_wolf_data_start = .;
        *(.data)
        . = ALIGN(4);
        *(.data.*)
        . = ALIGN(4);
        __rei_wolf_data_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note.*)
        *(.debug*)
        *(.fixup*)
        *(.gnu.attributes)
    }
}
EOF
} > "$OUT"
