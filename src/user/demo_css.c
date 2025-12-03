#include "system.h"

#include "minor_scene_bootstrapper.h"
#include "demo_css.h"
#include "alloc.h"

static MSB_CallbackExisting* callbacks = NULL;
static u16* think_count = NULL;

static bool has_displayed_css_think_message_yet = false;

void CSS_Think(void) {
    if (!think_count || !callbacks) {
        return;
    }

    if (*think_count < 5) {
        OSReport("[Rei Wolf]: think_count: %hu...", *think_count);
    } else if (*think_count == 5) {
        OSReport("[Rei Wolf]: think_count: %hu! Ok that's enough counting for now.", *think_count);
    }

    if (*think_count < 100) (*think_count)++; // Stop counting after thinking 100 times

    if (callbacks->MSB_ExistingThink) {
        callbacks->MSB_ExistingThink();
    }
}

void CSS_Load(void* data) {
    if (!think_count) {
        think_count = INJECT_Alloc(sizeof(*think_count));
        *think_count = 0;
    }

    if (callbacks->MSB_ExistingLoad) {
        callbacks->MSB_ExistingLoad(data);
    }
}

void CSS_Exit(void* data) {
    INJECT_Free(think_count);
    think_count = NULL;

    if (callbacks->MSB_ExistingExit) {
        callbacks->MSB_ExistingExit(data);
    }
}

void CSS_Bootstrap() {
    if (callbacks) {
        return;
    }

    callbacks = INJECT_Alloc(sizeof(MSB_CallbackExisting));
    MSB_Bootstrap(MINOR_SCENE_CSS, CSS_Think, CSS_Load, CSS_Exit, callbacks);
}
