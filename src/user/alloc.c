#include "system.h"
#include "alloc.h"

extern HeapDesc* stc_ptr_to_OSHeapArray;
extern int stc_max_number_of_heaps;

static u8* injection_heap_pointer = NULL;
static INJECT_SELECTION inject_selection = INJECT_SELECITON_ERROR;

void INJECT_ArenaLo_Init();

static INJECT_Region INJECT_Regions[] = {
    {
        (void*)0x8032C998,
        0x5E9C,
        "USB_MCC_EXTRA",
        NULL,
        INJECT_TYPE_STATIC
    },
    {
        NULL,
        0x100000,
        "INJECT_ARENA_LO_HEAP",
        INJECT_ArenaLo_Init,
        INJECT_TYPE_DYNAMIC
    }
};

void* INJECT_Alloc(size_t size) {
    int current_heap_id = HSD_GetHeapID();
    HSD_SetHeapID(INJECT_HEAP_ID);
    void* ptr = HSD_MemAlloc(size);
    HSD_SetHeapID(current_heap_id);
    return ptr;
}

void* INJECT_Free(void* ptr) {
    int current_heap_id = HSD_GetHeapID();
    HSD_SetHeapID(INJECT_HEAP_ID);
    HSD_Free(ptr);
    HSD_SetHeapID(current_heap_id);
}

void INJECT_ArenaLo_Init() {
    injection_heap_pointer = OSAllocFromArenaLo(INJECT_ALLOC_SIZE, 0x10);
}

void INJECT_Prepare(INJECT_SELECTION idx) {
    OSReport("[Rei Wolf]: Preparing heap (%s)...", INJECT_Regions[idx].name);
    inject_selection = idx;
    if (INJECT_Regions[idx].type == INJECT_TYPE_DYNAMIC) {
        INJECT_Regions[idx].init();
    } else if (INJECT_Regions[idx].type == INJECT_TYPE_STATIC) {
        injection_heap_pointer = INJECT_Regions[idx].start;
    }
    memset(injection_heap_pointer, '\0', INJECT_Regions[idx].size);
}

void INJECT_CreateHeapSpace() {
    Cell* cell;
    HeapDesc* heap;

    void* start = injection_heap_pointer;
    void* end = injection_heap_pointer + INJECT_Regions[inject_selection].size;

    start = (void*)(((u32)start + 3) & ~(3)); // round up 32-bit alignment
    end = (void*)((u32)end & ~0x03);          // round down 32-bit alignment

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

    OSReport("[Rei Wolf]: Injected Heap: %p - %p\n", start, start + INJECT_ALLOC_SIZE);
    OSReport("[Rei Wolf]: Injected Heap ID: %d\n", INJECT_HEAP_ID);

    HSD_SetHeapID(starting_heap_id);
}