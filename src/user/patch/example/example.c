#include "system.h"

#include "allocator/allocator.h"
#include "patch/patch.h"

// --------------------------------------------------------
// Examples for using the patch library

typedef void (*ExampleFunc)(int param);

static bool 
    PATCH_Example_FnHookCalled = false, 
    PATCH_Example_BranchHookCalled = false;

static PATCH_FnHook* example_fn_hook = NULL;
static PATCH_BranchResult* example_branch_results = NULL;

void PATCH_Example_Cleanup(void);

void PATCH_Example_BranchHook(void) {
    if (!PATCH_Example_BranchHookCalled) {
        OSReport("[Rei Wolf]: Branch hook called!\n");
        PATCH_Example_BranchHookCalled = true;
    }

    // If both branch and function hooks have been called, clean up
    if (PATCH_Example_FnHookCalled && PATCH_Example_BranchHookCalled)
        PATCH_Example_Cleanup();
}

void PATCH_Example_FunctionHook(int param) {
    if (!PATCH_Example_FnHookCalled) {
        OSReport("[Rei Wolf]: Function hook called with param: %d\n", param);
        PATCH_Example_FnHookCalled = true;
    }
    
    // Call original function via jump
    if (example_fn_hook)
        ((ExampleFunc)example_fn_hook->jump)(param);

    // If both branch and function hooks have been called, clean up
    if (PATCH_Example_FnHookCalled && PATCH_Example_BranchHookCalled)
        PATCH_Example_Cleanup();
}

void PATCH_Example_Patches(void) {
    // ----------------------------------------------------
    // Replace w/ real addresses when I have dolphin access
    #define PATCH_BRANCH_ADDR       ((void*)0x80012348)
    #define PATCH_FUNCTION_ADDR     ((void*)0x80067890)
    #define PATCH_INSTRUCTION_ADDR  ((void*)0x800ABCD4)
    #define PATCH_NOP_ADDR          ((void*)0x800DEF00)

    // ----------------------------------------------------
    // #1 Replace branch
    example_branch_results = PATCH_ReplaceBranch(
        PATCH_BRANCH_ADDR, 
        PATCH_Example_BranchHook
    );
    
    if (example_branch_results && example_branch_results->success)
        OSReport(
            "[Rei Wolf]: Redirected branch from %p to hook\n", 
            example_branch_results->original_target
        );
    
    // ----------------------------------------------------
    // #2 Function hook
    example_fn_hook = PATCH_HookFunction(
        PATCH_FUNCTION_ADDR,
        PATCH_Example_FunctionHook
    );
    
    if (example_fn_hook)
        OSReport(
            "[Rei Wolf]: Function hook installed at %p\n",
            PATCH_FUNCTION_ADDR
        );
    
    // ----------------------------------------------------
    // #3 Instruction replacement (`li r3, 1`)
    PATCH_Write32(PATCH_INSTRUCTION_ADDR, 0x38600001);
    
    // ----------------------------------------------------
    // #4 `nop`
    PATCH_NOP(PATCH_NOP_ADDR);
        
    #undef PATCH_BRANCH_ADDR
    #undef PATCH_FUNCTION_ADDR
    #undef PATCH_INSTRUCTION_ADDR
    #undef PATCH_NOP_ADDR
}

void PATCH_Example_Cleanup(void) {
    // Unhook both in cleanup

    if (example_branch_results) {
        OSReport("[Rei Wolf]: Cleaning up branch! (%p)!", example_branch_results->branch_address);
        PATCH_RestoreBranch(&example_branch_results);
    }
    
    if (example_fn_hook) {
        OSReport("[Rei Wolf]: Cleaning up function hook! (%p)!", example_fn_hook->original_function);
        PATCH_UnhookFunction(&example_fn_hook);
    }
}