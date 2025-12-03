#ifndef REI_WOLF_PATCH_H
#define REI_WOLF_PATCH_H

#include "system.h"

// Branch replacement results
typedef struct PATCH_BranchResult {
    void* branch_address;
    void* original_target;
    u32 original_instruction;
    bool success;
} PATCH_BranchResult;

// Function hooks
typedef struct PATCH_FnHook {
    void* original_function;
    void* hook_target;
    u32 saved[2];  // Original starting instructions
    u32 jump[8];   // Code to call original
} PATCH_FnHook;

// --------------------------------------------------------
// Inlined basic patching with cache management
static inline void PATCH_FlushCache(void* address, size_t size) {
    void* aligned_addr = (void*)((u32)address & ~31);
    size_t aligned_size = (size + 31) & ~31;
    DCFlushRange(aligned_addr, aligned_size);
    ICInvalidateRange(aligned_addr, aligned_size);
}

static inline void PATCH_Write8(void* address, u8 data) {
    *(volatile u8*)address = data;
    PATCH_FlushCache(address, 1);
}

static inline void PATCH_Write16(void* address, u16 data) {
    *(volatile u16*)address = data;
    PATCH_FlushCache(address, 2);
}

static inline void PATCH_Write32(void* address, u32 data) {
    *(volatile u32*)address = data;
    PATCH_FlushCache(address, 4);
}

static inline u32 PATCH_Read32(void* address) {
    return *(volatile u32*)address;
}

static inline void PATCH_NOP(void* address) {
    PATCH_Write32(address, 0x60000000); // ori r0, r0, 0
}

// --------------------------------------------------------
// Branch manipulation
void* PATCH_GetBranchTarget(void* branch_addr);
PATCH_BranchResult* PATCH_ReplaceBranch(void* branch_addr, void* new_target);
void PATCH_RestoreBranch(PATCH_BranchResult** result);

// --------------------------------------------------------
// Function hooking
PATCH_FnHook* PATCH_HookFunction(void* target_function, void* hook_function);
void PATCH_UnhookFunction(PATCH_FnHook** hook);

#endif // REI_WOLF_PATCH_H