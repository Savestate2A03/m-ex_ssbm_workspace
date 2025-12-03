# m-ex_pic_workspace
A work-in-progress environment for generating PIC (position-independent code) with m-ex. The environment assumes that you have devkitPro installed, the `powerpc-eabi-*` toolchain along with it. I would like to be able to be a type of hook-and-forget environment that handles the tedious parts of modding for you. As I work on this, it will develop into a more featured type of thing hopefully.

# Building
Run `make` within a devkitPro-specific msys2 terminal

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

## Static variables are now `extern`

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

Other symbols were added as well that didn't previously exist. The linker takes care of referencing them, utilizing the linker generation script `generate_linker_script.sh` that uses `melee.link` as an input file. This is part of the repo here, not m-ex.

```ld
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
