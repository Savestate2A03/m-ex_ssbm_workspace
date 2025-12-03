#include "system.h"

#include "allocator/allocator.h"
#include "patch/patch.h"

#include "minor_scene/bootstrapper.h"
#include "minor_scene/scenes/css.h"

#include "patch/example/example.h"

// TO BE INJECTED INTO JUMP TO `main` from _init @ 0x80005338

/** 
 *  A work-in-progress environment for generating PIC (position-independent code) with m-ex.
 *  The environment assumes you have devkitPro installed along with the powerpc-eabi-* tool-
 *  -chain. I'd hope for this to turn into a hook-and-forget environment that can handle the
 *  tedious parts of injection modding for you. As I work on this, it'll develop into a more
 *  featured type of thing hopefully. 
 */

// .got2 references
extern u32 __rei_wolf_got2_lo[];
extern u32 __rei_wolf_got2_hi[];

// Static array of function pointers to run after _patch
static void* _patch_inject_init_va[] = {
    INJECT_CreateHeapSpace,
    CSS_Bootstrap,
    PATCH_Example_Patches,
    NULL
};

// Repoints the pointers to static data in .got2 to their runtime equivalents
// while making sure that pointers to Melee-space static data does not change
void _relocate_got2_table(void) {
    extern u8 __rei_wolf_text_lo[];
    size_t offset = (size_t)__rei_wolf_text_lo - 0x7C000000;
    
    for (u32* entry = __rei_wolf_got2_lo; entry < __rei_wolf_got2_hi; entry++) {
        if ((*entry >> 26) == 0x1F) {
            *entry += offset;
        }
    }
}

// Run the list of functions that _patch() sets up to happen later on
void _init() {
    void (*fn)() = NULL;
    size_t idx = 0;
    while ((fn = _patch_inject_init_va[idx++])) { fn(); }
}

__attribute__((section(".text._patch")))
void _patch() {

    // Relocate .got2 entries
    _relocate_got2_table();

    /** 
     *  Set stc_max_number_of_heaps to 7 (ie. INJECT_HEAP_ID + 1) so it
     *  doesn't get destroyed by the heap library (which will wipe 2-5)
     *
     *  Afterwards, patch the function parameter that sets stc_max_number_of_heaps.
     *  
     *  (There is both a static variable already set to 4 and this function call
     *  that additionally sets it to 4. Why it is this way, do not ask me!)
     *  
     *  before : li r4, 4
     *  after  : li r4, 7
     */
    extern int stc_max_number_of_heaps;
    stc_max_number_of_heaps = INJECT_HEAP_ID + 1;
    PATCH_Write8((void*)0x8015ff63, stc_max_number_of_heaps);

    // Prepare the heap
    INJECT_Prepare(USB_MCC_EXTRA);

    // Hook into major scene init
    MajorScene* majorList = (MajorScene*)Scene_GetMajorSceneDesc();
    majorList[MJRKIND_DBGEND] = (MajorScene){
        .is_preload = false,
        .major_id = MJRKIND_DBGEND,
        .cb_Load = NULL,
        .cb_Exit = NULL,
        .cb_Boot = _init // Hitch a ride on major scene init, uses the debug
                         // ending major scene init function pointer feel free
                         // to suggest a better place in the init process lol
    };
}