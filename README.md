# m-ex_pic_worspace
Generates PIC with m-ex

Run `make` within a devkitPro msys2 context

# Changes around m-ex
Several changes were made to m-ex. You can view them all [here](https://github.com/Savestate2A03/m-ex/commits/master/). A lot of cleanup was done such as adhering to C23 and filling references that weren't defined. 

## Many library functions added

A lot of the built-in library functions for Melee were added/updated/fixed/uncommented. For example, this is in `useful.h`:

```c
/** Library Functions **/
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t size);
char *strchr(const char *str, char c); // searches for the first occurrence of the character c (an unsigned char) in the string pointed to by the argument str.
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t size);
int strlen(const char *str);
int sprintf(char *s, const char *format, ...);
int vsnprintf(char *s, size_t n, const char *format, va_list arg);
#define vsprintf(s, fmt, arg) vsnprintf((s), ULONG_MAX, (fmt), (arg))
unsigned int strtoul(const char *startptr, char **endptr, int base); // string to unsigned long, actually 32-bits

char* toupper(char c); // returns a pointer to the upper/lower character in a lookup table
char* tolower(char c); // returns a pointer to the upper/lower character in a lookup table

void* memset(void *ptr, int value, int bytes);
void* memcpy(void *restrict s1, const void *restrict s2, size_t size);
int memcmp(const void *s1, const void *s2, size_t n);
void* memmove(void *dest, const void *src, size_t n);

float sqrtf(float x);
float powf(float base, float exponent);
int powi(int base, int exponent);
double tan(double x);
double cos(double x);
double sin(double x);
double log(double x);
double fabs(double x);
double atan(double x);
double atan2(double y, double x);
double fmod(double x, double y);
int rand(void);
double frexp(double x, int *exp);
```

## Static variables changed to `extern`

This allows referencing them directly instead of taking up space as static variables. The headers would include them in their compilation units.
```diff
  // variables
- static ClassicLineup *stc_clsc_lineup = 0x803ddec8;
- static u8 *stc_clsc_shuffledorder1 = 0x80490900; // 9 elements
- static u8 *stc_clsc_shuffledorder2 = 0x804908f4; // 10 elements
- static u8 *stc_clsc_shuffledorder3 = 0x804908d4; // 27 elements
- static u8 *stc_clsc_shuffledorder4 = 0x804908ac; // 38 elements
+ extern ClassicLineup stc_clsc_lineup;
+ extern u8 stc_clsc_shuffledorder1[9];
+ extern u8 stc_clsc_shuffledorder2[10];
+ extern u8 stc_clsc_shuffledorder3[27];
+ extern u8 stc_clsc_shuffledorder4[38];
```

This is possible by now having `melee.link` house static variables:

```
...
80479D58:stc_hsd_update
804908AC:stc_clsc_shuffledorder4
804908D4:stc_clsc_shuffledorder3
804908F4:stc_clsc_shuffledorder2
80490900:stc_clsc_shuffledorder1
8049E6C8:stc_stage
804A0BC0:stc_css_cursors
804A0BD0:stc_css_pucks
...
```

Other symbols were added as well that didn't previously exist.

The linker takes care of referencing them, utilizing the linker generation script `generate_linker_script.sh` that uses `melee.link` as an input file. This is part of the repo here, not m-ex.

```sh
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
    . = 0x80000000;

    .text : {
        *(.text)
# ...
```

# Project Layout

Here's a general overview of the project:

- `system/_start.s` relocates the .got2 symbol table and calls _patch() from start.c
- `extract_hex.py` generates nice asm tool friendly assembly
- `include/system.h` wraps the m-ex library in a `#pragma longcall(1)` so that all branches to system functions are done with `brctl` and all references to static variables are loaded directly instead of relative
- `_patch()` creates space to create a heap we can register with the operating system later on. the current hook process is a bit silly but works

# dedicated heap allocation

its pretty nifty. the system never touches it. it's all ours baby. check it out in alloc.c and the initial patch in start.c

## longcall
You can see how this occurs in the generated assembly for the example project:

### Local calls
Local calls are handled with simple branches to labels:
```asm
# ...
cmpwi   r9, 0
bne     start_0x4c
lwz     r0, 0x34(r1)
lwz     r30, 0x28(r1)
lwz     r31, 0x2c(r1)
mtlr    r0
addi    r1, r1, 48
blr
start_0x4c:
lwz     r5, -0x7ff8(r30)
lwz     r6, -0x7ff4(r30)
# ...
```

Local static variables are located at `__rei_wolf_got2_start`, they are relocated to the actual space of the injected code in the _start script.

```asm

_relocate_got2_table:

    mflr r27
    bl _relocate_got2_table_context

_relocate_got2_table_context:

    mflr r22
    lis  r21, _relocate_got2_table_context@ha
    addi r21, r21, _relocate_got2_table_context@l

    subf r22, r21, r22

    lis  r23, __rei_wolf_got2_start@ha
    addi r23, r23, __rei_wolf_got2_start@l
    add  r23, r23, r22

    lis  r24, __rei_wolf_got2_end@ha
    addi r24, r24, __rei_wolf_got2_end@l
    add  r24, r24, r22

_relocate_got2_table_got2_loop:

    cmpw  r23, r24
    bge   _relocate_got2_table_got2_loop_done

    lwz   r25, 0(r23)

    # skip melee static pointers @ 0x8000xxxx
    lis   r26, 0x8000
    and.  r26, r26, r25
    bne   _relocate_got2_table_got2_next

    add   r25, r25, r22 # relocate entry
    stw   r25, 0(r23)

_relocate_got2_table_got2_next:
    addi  r23, r23, 4
    b     _relocate_got2_table_got2_loop

_relocate_got2_table_got2_loop_done:
    mtlr r27
    blr

```


### Long calls
Calls to library functions are loaded from the `__rei_wolf_got2_start` section and branched to with `bctrl`.

```c
void EXAMPLE_my_example_destroy(EXAMPLE_Data** data) {
    HSD_Free((void*)(*data)->example_string);
    (*data)->example_string = NULL;
    HSD_Free(*data);
    *data = NULL;
}
```

```asm
EXAMPLE_my_example_destroy:
stwu    r1, -0x18(r1)
mflr    r0
bcl     20, 31, EXAMPLE_my_example_destroy_0xc
EXAMPLE_my_example_destroy_0xc:
lwz     r9, 0x0(r3)
stw     r28, 0x8(r1)
stw     r29, 0xc(r1)
li      r29, 0
stw     r30, 0x10(r1)
mflr    r30
stw     r31, 0x14(r1)
mr      r31, r3
stw     r0, 0x1c(r1)
lwz     r3, 0x14(r9)
lwz     r0, -0x10(r30)
add     r30, r0, r30
lwz     r28, -0x7fe8(r30)
mtctr   r28
bctrl
lwz     r3, 0x0(r31)
mtctr   r28
stw     r29, 0x14(r3)
bctrl
lwz     r0, 0x1c(r1)
stw     r29, 0x0(r31)
lwz     r28, 0x8(r1)
mtlr    r0
lwz     r29, 0xc(r1)
lwz     r30, 0x10(r1)
lwz     r31, 0x14(r1)
addi    r1, r1, 24
blr
.long 0x000081b8
```

Similarly, references to static variables are done this way

```c
void MODIFY_up_the_volume() {
    stc_match_bgm_volume++;
}
```

```asm
MODIFY_up_the_volume:
stwu    r1, -0x10(r1)
mflr    r0
bcl     20, 31, MODIFY_up_the_volume_0xc
MODIFY_up_the_volume_0xc:
stw     r30, 0x8(r1)
mflr    r30
stw     r0, 0x14(r1)
lwz     r0, -0x10(r30)
add     r30, r0, r30
lwz     r0, 0x14(r1)
lwz     r9, -0x8000(r30)
lwz     r10, -0x7ffc(r30)
mtlr    r0
lfs     f0, 0x0(r9)
lfs     f12, 0x0(r10)
lwz     r30, 0x8(r1)
fadds   f0, f0, f12
stfs    f0, 0x0(r9)
addi    r1, r1, 16
blr
.long 0x00008170
```

# Compiling

## Flags

Here's the flags used when compiling, defined in `makefile`:

```make
CFLAGS := -DGEKKO -mogc -mcpu=750         # Target Gekko
CFLAGS += -meabi                          # Embedded ABI calling convention
CFLAGS += -mhard-float                    # Hardware floating point
CFLAGS += -O2                             # Optimization lvl 2
CFLAGS += -mno-sdata                      # Disable "small data area optimization" (Don't use r2/r13)
CFLAGS += -G0                             # Small data threshold to 0 bytes (Just in case...)
CFLAGS += -std=c23                        # C23 standard
CFLAGS += -fPIC                           # Position-independent code
CFLAGS += -fno-asynchronous-unwind-tables # No .eh_frame_hdr (exception handling metadata section)
CFLAGS += -fno-unwind-tables              # No .eh_frame (stack unwinding section)
CFLAGS += -fno-zero-initialized-in-bss    # Zero-init globals in .data (not .bss)
CFLAGS += -fno-data-sections              # All data in .data (not i.e. .data.varname)
CFLAGS += -fno-function-sections          # All code in .text (not i.e. .text.funcname)
CFLAGS += -ffreestanding                  # No standard library
CFLAGS += -fno-hosted                     # Bare metal
CFLAGS += $(CFLAGS_EXTRAS) $(INCLUDE_DIR)
```

## Running `make`
Here's what is generated with the current example project:

```
rei wolf@DESKTOP-8JOF1RA MSYS /g/dev/test_mods
$ make
Assembling: system/_start.s
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-as -mregnames -mgekko system/_start.s -o build/system/_start.o
Compiling: src/example.c
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -c -DGEKKO -mogc -mcpu=750 -meabi -mhard-float -O2 -mno-sdata -G0 -std=c23 -fPIC -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-zero-initialized-in-bss -fno-data-sections -fno-function-sections -ffreestanding -fno-hosted -I./m-ex/MexTK/include -I./include/system -I./include/src src/example.c -o build/src/example.o
Compiling: src/modify_static_variable.c
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -c -DGEKKO -mogc -mcpu=750 -meabi -mhard-float -O2 -mno-sdata -G0 -std=c23 -fPIC -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-zero-initialized-in-bss -fno-data-sections -fno-function-sections -ffreestanding -fno-hosted -I./m-ex/MexTK/include -I./include/system -I./include/src src/modify_static_variable.c -o build/src/modify_static_variable.o
Compiling: src/start.c
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -c -DGEKKO -mogc -mcpu=750 -meabi -mhard-float -O2 -mno-sdata -G0 -std=c23 -fPIC -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-zero-initialized-in-bss -fno-data-sections -fno-function-sections -ffreestanding -fno-hosted  -I./m-ex/MexTK/include -I./include/system -I./include/src src/start.c -o build/src/start.o
Assembling: system/_magic.s
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-as -mregnames -mgekko system/_magic.s -o build/system/_magic.o
system/_magic.s: Assembler messages:
system/_magic.s: Warning: end of file in comment; newline inserted
Generating linker script...
Linking...
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-ld ./build/system/_start.o  ./build/src/example.o ./build/src/modify_static_variable.o ./build/src/start.o ./build/system/_magic.o -o build/output/inject.elf -T ./build/src/melee.ld -Map=./build/output/inject.map -nostdlib -e _start
Creating binary...
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objcopy -O binary --only-section=.text build/output/inject.elf build/output/inject.bin
Binary size: 1324 bytes
Creating disassembly...
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objdump -D --disassemble-zeroes --insn-width=4 build/output/inject.elf > build/output/inject.asm
Creating hex disassembly...
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objdump -s --disassemble-zeroes build/output/inject.elf > build/output/inject_hex.asm
```

## Output

Output is handled with `generate_hex.py`, which uses files built defined by `makefile` to generate the ASM and binary blob.

### ASM
For pasting into a code injection tool
```asm
_start:
mflr    r0
stw     r0, 0x4(r1)
stwu    r1, -0x100(r1)
stmw    r3, 0x8(r1)
bl      start
lmw     r3, 0x8(r1)
lwz     r0, 0x104(r1)
addi    r1, r1, 256
# ... etc ...
.long 0x40400000
.long 0x40000000
.long 0x00000000
.long 0x00000900
```

### BIN
For the actual binary blob of generated code
```
7c0802a6 90010004
9421ff00 bc610008
48000399 b8610008
80010104 38210100
# ... etc ...
3f800000 40a00000
40800000 40400000
40000000 00000000
00000900
```
