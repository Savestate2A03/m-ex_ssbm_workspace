#include "system.h"

#include "alloc.h"
#include "demo_css.h"
#include "minor_scene_bootstrapper.h"

inline void _patch_u32(u32* address, u32 data) { *address = data; }
inline void _patch_u16(u16* address, u16 data) { *address = data; }
inline void _patch_u8(u8* address, u8 data) { *address = data; }

static bool bootstrapped = false;

void start() {
    if (!bootstrapped) { // just in case ???
        INJECT_CreateHeapSpace();
        _breakpoint_helper(1);
        CSS_Bootstrap();
        bootstrapped = true;
    }
}

void _patch() {
    // max heaps to 7 (ie. INJECT_HEAP_ID) so it doesn't get
    // destroyed by the heap library (which checks for 2-5)
    stc_max_number_of_heaps = INJECT_HEAP_ID + 1;

    // before : li r4, 4
    // after  : li r4, 7
    _patch_u8((void*)0x8015ff63, stc_max_number_of_heaps);

    // nudge up arenalo, we will utilize this space later during major scene init
    void** injection_heap_pointer = INJECT_GetHeapPointer();
    *injection_heap_pointer = OSAllocFromArenaLo(INJECT_ALLOC_SIZE, 0x10);

    MajorScene* majorList = (MajorScene*)Scene_GetMajorSceneDesc();

    majorList[MJRKIND_DBGEND] = (MajorScene){
        .is_preload = false,
        .major_id = MJRKIND_DBGEND,
        .cb_Load = NULL,
        .cb_Exit = NULL,
        .cb_Boot = start // Hitch a ride on major scene init, uses the debug
                         // ending major scene init function pointer feel free
                         // to suggest a better place in the init process lol
    };
}
