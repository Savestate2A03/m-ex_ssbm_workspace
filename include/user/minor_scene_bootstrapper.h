#ifndef REI_WOLF_MINOR_SCENE_BOOTSTRAPPER
#define REI_WOLF_MINOR_SCENE_BOOTSTRAPPER

#include "system.h"

typedef void (*MSB_Think)(void);
typedef void (*MSB_Load)(void* data);
typedef void (*MSB_Exit)(void* data);

typedef struct MSB_CallbackExisting {
    MSB_Think MSB_ExistingThink;
    MSB_Load MSB_ExistingLoad;
    MSB_Exit MSB_ExistingExit;
    bool error;
} MSB_CallbackExisting;

typedef enum MinorSceneDescID {
    MINOR_SCENE_00 = 0x00,
    MINOR_SCENE_01,
    MINOR_SCENE_02,
    MINOR_SCENE_03,
    MINOR_SCENE_04,
    MINOR_SCENE_05,
    MINOR_SCENE_06,
    MINOR_SCENE_07,
    MINOR_SCENE_CSS,
    MINOR_SCENE_08,
    // ...
    MINOR_SCENE_2C = 0x2C,
    MINOR_SCENE_MAX = 0x2D
} MinorSceneDescID;

void MSB_Bootstrap(u8 id, MSB_Think think, MSB_Load load, MSB_Exit exit, MSB_CallbackExisting* callbacks);
bool MSB_Restore(u8 id);

#endif // REI_WOLF_MINOR_SCENE_BOOTSTRAPPER