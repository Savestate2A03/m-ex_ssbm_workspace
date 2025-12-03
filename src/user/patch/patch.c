#include "system.h"
#include "allocator/allocator.h"
#include "patch/patch.h"

// --------------------------------------------------------
// Get the address of a branch instruction
void* PATCH_GetBranchTarget(void* branch_addr) {
    u32 instruction = PATCH_Read32(branch_addr);
    u8 opcode = (instruction >> 26) & 0x3F;
    s32 offset;

    // Unconditional branches
    if (opcode != 18) return NULL;
    
    // Get 24-bit signed offset, sign-extend to 32 bits
    if ((offset = (instruction & 0x03FFFFFC)) & 0x02000000) offset |= 0xFC000000;
    
    return (void*)((u32)branch_addr + offset);
}

// --------------------------------------------------------
// Replace a branch instruction with a new target
PATCH_BranchResult* PATCH_ReplaceBranch(void* branch_addr, void* new_target) {
    
    // Allocate branch info
    PATCH_BranchResult* results = INJECT_Alloc(sizeof(PATCH_BranchResult));

    if (!results) {
        OSReport("[Rei Wolf]: (ERROR) Failed to allocate for branch hook...\n");
        return NULL;
    }

    results->branch_address = branch_addr,
    results->original_target = NULL;
    results->original_instruction = 0;
    results->success = false;
    
    if (!branch_addr || !new_target) return results;
    
    // Save original instruction and get its target
    results->original_instruction = PATCH_Read32(branch_addr);
    results->original_target = PATCH_GetBranchTarget(branch_addr);
    
    if (!results->original_target) {
        OSReport("[Rei Wolf]: (ERROR) %p not a valid branch\n", branch_addr);
        return results;
    }
    
    // Get offset
    s32 offset = (s32)new_target - (s32)branch_addr;
    
    // Check offset range
    if (offset < -0x2000000 || offset > 0x1FFFFFF) {
        OSReport("[Rei Wolf]: (ERROR) Branch offset range: %d\n", offset);
        return results;
    }
    
    // 4-byte alignment, preserve LK bit
    offset &= 0x03FFFFFC;
    u32 new_instruction = 0x48000000 | offset | (results->original_instruction & 1);
    
    // Write branch
    PATCH_Write32(branch_addr, new_instruction);
    
    results->success = true;
    
    OSReport(
        "[Rei Wolf]: Replaced branch at %p: %p -> %p\n", 
        branch_addr,
        results->original_target,
        new_target
    );
    
    return results;
}

// --------------------------------------------------------
// Restore a branch to its original instruction
void PATCH_RestoreBranch(PATCH_BranchResult** results) {
    // Sanity check results
    if (!results || !*results || !(*results)->success || !(*results)->branch_address) {
        OSReport("[Rei Wolf]: (ERROR) Bad branch results to restore\n");
        return;
    }
    
    // Restore original
    PATCH_Write32((*results)->branch_address, (*results)->original_instruction);
    
    OSReport(
        "[Rei Wolf]: Restored branch at %p to original target %p\n",
        (*results)->branch_address,
        (*results)->original_target
    );
    
    (*results)->success = false;

    INJECT_Free(*results);
    *results = NULL;
}

// --------------------------------------------------------
// Function hooking
PATCH_FnHook* PATCH_HookFunction(void* target_function, void* hook_function) {
    if (!target_function || !hook_function) return NULL;
    
    // Allocate hook info
    PATCH_FnHook* hook = INJECT_Alloc(sizeof(PATCH_FnHook));

    if (!hook) {
        OSReport("[Rei Wolf]: (ERROR) Failed to allocate for function hook...\n");
        return NULL;
    }
    
    hook->original_function = target_function;
    hook->hook_target = hook_function;
    
    // Get start of function (usually something like `mflr r0`, `stw r0, 4(r1)`)
    hook->saved[0] = PATCH_Read32(target_function);
    hook->saved[1] = PATCH_Read32((void*)((u32)target_function + 4));
    
    // Build instructions to call original function. 
    // (run the prologue and jump past the hook)
    u32* jump = hook->jump;
    
    // Run saved instructions
    jump[0] = hook->saved[0];
    jump[1] = hook->saved[1];
    
    // Branch to original function + 0x8 (skip prologue)
    void* return_addr = (void*)((u32)target_function + 8);
    s32 offset = (s32)return_addr - (s32)(&jump[2]);
    
    if (offset < -0x2000000 || offset > 0x1FFFFFF) {
        OSReport("[Rei Wolf]: (ERROR) Jump offset out of range!\n");
        INJECT_Free(hook);
        return NULL;
    }
    
    offset &= 0x03FFFFFC;
    jump[2] = 0x48000000 | offset;  // b to original + 0x8
    
    // Flush jump cache
    PATCH_FlushCache(jump, sizeof(hook->jump));
    
    // Create branch to hook function
    offset = (s32)hook_function - (s32)target_function;
    
    if (offset < -0x2000000 || offset > 0x1FFFFFF) {
        OSReport("[Rei Wolf]: (ERROR) Function hook offset out of range\n");
        INJECT_Free(hook);
        return NULL;
    }
    
    offset &= 0x03FFFFFC;
    u32 branch_to_hook = 0x48000000 | offset;  // b to hook
    
    // Replace prologue with branch to hook + nop
    PATCH_Write32(target_function, branch_to_hook);
    PATCH_Write32((void*)((u32)target_function + 4), 0x60000000); // nop
    
    OSReport("[Rei Wolf]: Hooked function at %p -> %p\n", target_function, hook_function);
    OSReport("[Rei Wolf]: Jump at %p\n", jump);
    
    return hook;
}

// --------------------------------------------------------
// Unhook function
void PATCH_UnhookFunction(PATCH_FnHook** hook) {
    if (!hook || !*hook) return;
    
    // Restore original prologue
    PATCH_Write32((*hook)->original_function, (*hook)->saved[0]);
    PATCH_Write32((void*)((u32)(*hook)->original_function + 4), (*hook)->saved[1]);
    
    OSReport("[Rei Wolf]: Unhooked function at %p\n", (*hook)->original_function);
    
    INJECT_Free(*hook);
    *hook = NULL;
}