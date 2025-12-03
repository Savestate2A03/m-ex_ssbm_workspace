#include "system.h"

#include "alloc.h"
#include "demo_css.h"
#include "minor_scene_bootstrapper.h"

// to be injected into jump to main from __start, 0x80005338

void _start();

inline void _patch_u32(u32* address, u32 data) { *address = data; }
inline void _patch_u16(u16* address, u16 data) { *address = data; }
inline void _patch_u8(u8* address, u8 data) { *address = data; }

extern u32 __rei_wolf_got2_start[];
extern u32 __rei_wolf_got2_end[];

static void* inject_init_va[] = {
    INJECT_CreateHeapSpace,
    CSS_Bootstrap,
    NULL
};

__attribute__((section(".text.__patch")))
void __patch() {
    // max heaps to 7 (ie. INJECT_HEAP_ID) so it doesn't get
    // destroyed by the heap library (which checks for 2-5)
    stc_max_number_of_heaps = INJECT_HEAP_ID + 1;

    // before : li r4, 4
    // after  : li r4, 7
    _patch_u8((void*)0x8015ff63, stc_max_number_of_heaps);

    INJECT_Prepare(USB_MCC_EXTRA);

    MajorScene* majorList = (MajorScene*)Scene_GetMajorSceneDesc();

    majorList[MJRKIND_DBGEND] = (MajorScene){
        .is_preload = false,
        .major_id = MJRKIND_DBGEND,
        .cb_Load = NULL,
        .cb_Exit = NULL,
        .cb_Boot = _start // Hitch a ride on major scene init, uses the debug
                          // ending major scene init function pointer feel free
                          // to suggest a better place in the init process lol
    };
}

__attribute__((noinline))
void _relocate_got2_table_context(void) {
    // Get our actual runtime address
    void *actual_addr = __builtin_return_address(0);
    
    // Get our expected (linked) address
    u32* expected_addr = (u32*)_relocate_got2_table_context;
    
    // Calculate relocation offset
    size_t offset = (u32*)actual_addr - expected_addr;
    
    // Apply offset to GOT2 section boundaries
    u32 *got2_start = (u32*)((u32*)__rei_wolf_got2_start + offset);
    u32 *got2_end = (u32*)((u32*)__rei_wolf_got2_end + offset);
    
    // Iterate through GOT2 entries
    for (u32 *entry = got2_start; entry < got2_end; entry++) {
        u32 value = *entry;
        
        // Skip Melee static pointers (0x8000xxxx range)
        if (!(value & 0x80000000)) {
            *entry = value + offset;
        }
    }
}

__attribute__((noinline))
void _relocate_got2_table(void) {
    _relocate_got2_table_context();
}

void _start() {
    void (*fn)() = NULL;
    size_t idx = 0;
    while ((fn = inject_init_va[idx])) {
        fn();
    }
}