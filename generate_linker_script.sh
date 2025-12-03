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
    . = 0x7C000000;

    __rei_wolf_text_lo = .;

    .text : {
        *(.text._patch)
        *(.text)
        *(.text.*)
        . = ALIGN(4);

        __rei_wolf_rodata_lo = .;
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        __rei_wolf_rodata_hi = .;

        __rei_wolf_got2_lo = .;
        *(.got2)
        . = ALIGN(4);
        __rei_wolf_got2_hi = .;

        __rei_wolf_data_lo = .;
        *(.data)
        . = ALIGN(4);
        *(.data.*)
        . = ALIGN(4);
        __rei_wolf_data_hi = .;

        __rei_wolf_pad_lo = .;
        . = ALIGN(8);
        __rei_wolf_pad_hi = .;
    }

    __rei_wolf_text_hi = .;

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
