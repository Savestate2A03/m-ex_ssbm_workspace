#include "system.h"

#include "allocator/allocator.h"
// --------------------------------------------------------
// Forward decleration
void INJECT_ArenaLo_Init();

// --------------------------------------------------------
// Static variables we want to keep track of between calls
extern HeapDesc* stc_ptr_to_OSHeapArray;
static u8* injection_heap_pointer = NULL;
static INJECT_SELECTION inject_selection = INJECT_SELECTION_ERROR;

static INJECT_Region INJECT_Regions[] = {
    (INJECT_Region){
        .start = (void*)USB_MCC_EXTRA_ADDR,
        .size = 0x5E9C,
        .name = "USB_MCC_EXTRA",
        .init = NULL,
        .type = INJECT_TYPE_STATIC
    },
    (INJECT_Region){
        .start = NULL,
        .size = 0x100000,
        .name = "INJECT_ARENA_LO_HEAP",
        .init = INJECT_ArenaLo_Init,
        .type = INJECT_TYPE_DYNAMIC
    }
};

// --------------------------------------------------------
// Allocate memory, switch to our heap before, switch back after
void* INJECT_Alloc(size_t size) {
    if (!injection_heap_pointer) {
        OSReport("[Rei Wolf]: ERROR - Heap not initialized!\n");
        return NULL;
    }
    int current_heap_id = HSD_GetHeapID();
    HSD_SetHeapID(INJECT_HEAP_ID);
    void* ptr = HSD_MemAlloc(size);
    HSD_SetHeapID(current_heap_id);
    return ptr;
}

// --------------------------------------------------------
// Free memory, same pattern
void INJECT_Free(void* ptr) {
    int current_heap_id = HSD_GetHeapID();
    HSD_SetHeapID(INJECT_HEAP_ID);
    HSD_Free(ptr);
    HSD_SetHeapID(current_heap_id);
}

// --------------------------------------------------------
// Allocate memory at the start before the OS can dedicate it to the game.
// This is only done if calling INJECT_Prepare(idx) with INJECT_ARENA_LO_HEAP.
void INJECT_ArenaLo_Init() {
    injection_heap_pointer = OSAllocFromArenaLo(INJECT_ALLOC_SIZE, 0x10);
}

// --------------------------------------------------------
// Prepare memory for us, either using a fixed location or calling an init
// function that prepares the memory for us before hand if it's dynamic.
void INJECT_Prepare(INJECT_SELECTION idx) {
    if (idx >= INJECT_SELECTION_ERROR) {
        OSReport("[Rei Wolf]: Invalid heap selection!\n");
        return;
    }
    OSReport("[Rei Wolf]: Preparing heap (%s)...", INJECT_Regions[idx].name);
    inject_selection = idx;
    if (INJECT_Regions[idx].type == INJECT_TYPE_DYNAMIC) {
        INJECT_Regions[idx].init();
    } else if (INJECT_Regions[idx].type == INJECT_TYPE_STATIC) {
        injection_heap_pointer = INJECT_Regions[idx].start;
    }
    memset(injection_heap_pointer, '\0', INJECT_Regions[idx].size);
}

// --------------------------------------------------------
// Initalize the actual heap structure for our entry in the OSHeapArray that
// we have tricked HSD into making 7 entries long. HSD will manage our memory.
void INJECT_CreateHeapSpace() {
    Cell* cell;
    HeapDesc* heap;

    void* start = injection_heap_pointer;
    void* end = injection_heap_pointer + INJECT_Regions[inject_selection].size;

    start = (void*)(((u32)start + 7) & ~7); // 8-byte boundary round up
    end = (void*)((u32)end & ~7);           // 8-byte boundary round down

    if ((u32)end <= (u32)start) {
        OSReport("[Rei Wolf]: (ERROR) No more space after alignment!\n");
        return;
    }

    heap = &(stc_ptr_to_OSHeapArray[INJECT_HEAP_ID]); 

    heap->size = (u32)end - (u32)start;
    cell = start;
    cell->prev = NULL;
    cell->next = NULL;
    cell->size = heap->size;
    heap->free = cell;
    heap->allocated = NULL;

    int starting_heap_id = HSD_GetHeapID();
    HSD_SetHeapID(INJECT_HEAP_ID);

    OSReport("[Rei Wolf]: Injected Heap: %p - %p (%u bytes)\n", start, end, heap->size);
    OSReport("[Rei Wolf]: Injected Heap ID: %d\n", INJECT_HEAP_ID);

    HSD_SetHeapID(starting_heap_id);
}