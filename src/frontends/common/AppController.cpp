#include "frontends/common/AppController.h"
#include "frontends/common/AppArgs.h"
#include "frontends/common/AppEnvironment.h"
#include "core/LinAppleCore.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "core/Log.h"
#include "core/ProgramLoader.h"
#include "core/Peripheral_Internal.h"
#include "core/Util_Text.h"
#include "apple2/SaveState.h"
#include "apple2/Video.h"
#include <cstdio>

static bool s_initialized = false;

int AppController_Initialize(AppConfig* config) {
    if (!config) return -1;
    
    // Idempotency: ensure we start from a clean state if called multiple times
    if (s_initialized) {
        AppController_Shutdown();
    }

    // 1. Resolve paths and init Registry/Logger
    AppEnv_ResolvePaths(config);

    if (config->intent == INTENT_HELP) {
        AppArgs_PrintHelp();
        return 0;
    }

    if (config->intent == INTENT_DIAGNOSTIC) {
        if (config->bListHardware) {
            Linapple_ListHardware();
            return 0;
        }
        if (config->szHardwareInfoName[0] != '\0') {
            Peripheral_t* p = Peripheral_Find_Internal(config->szHardwareInfoName);
            if (p) {
                printf("Hardware Info: %s\n", p->name);
                printf("ABI Version: %d\n", p->abi_version);
                printf("Compatible Slots: ");
                bool first = true;
                for (int i = 0; i < NUM_SLOTS; ++i) {
                  if (p->compatible_slots & (1u << static_cast<uint32_t>(i))) {
                    if (!first) printf(", ");
                    printf("%d", i);
                    first = false;
                  }
                }
                printf("\n");
                const char* path = Peripheral_GetPluginPath(config->szHardwareInfoName);
                if (path) {
                    printf("Plugin Path: %s\n", path);
                }
            } else {
                fprintf(stderr, "Error: Unknown hardware '%s'\n", config->szHardwareInfoName);
                return 1;
            }
            return 0;
        }
        if (config->szTestCpuFile[0] != '\0') {
            Linapple_CpuTest(config->szTestCpuFile, config->uTestCpuTrap);
            return 0;
        }
    }

    // 2. Init Core
    Linapple_Init();
    s_initialized = true;

    // 3. Set Hardware Type and PAL
    g_Apple2Type = config->apple2Type;
    if (config->bPAL) {
        g_videotype = VT_COLOR_TVEMU;
    }

    // 4. Init Snapshots
    if (config->szSnapshotPath[0] != '\0') {
        Snapshot_SetFilename(config->szSnapshotPath);
    }
    Snapshot_Startup();

    // 5. Load Disks into Registry (for Peripherals)
    if (config->szDiskPath[0][0] != '\0') {
        // We use Linapple_LoadProgram for disks because it checks if it's a program first
        if (Linapple_LoadProgram(config->szDiskPath[0]) == PROGRAM_LOAD_NOT_A_PROGRAM) {
            Configuration::Instance().SetString("Slots", REGVALUE_DISK_IMAGE1, config->szDiskPath[0]);
        }
    }
    if (config->szDiskPath[1][0] != '\0') {
        if (Linapple_LoadProgram(config->szDiskPath[1]) == PROGRAM_LOAD_NOT_A_PROGRAM) {
            Configuration::Instance().SetString("Slots", REGVALUE_DISK_IMAGE2, config->szDiskPath[1]);
        }
    }

    // 6. Register Peripherals
    Linapple_RegisterPeripherals();

    // 7. Load Program
    if (config->szProgramPath[0] != '\0') {
        if (Linapple_LoadProgram(config->szProgramPath) != 0) {
            fprintf(stderr, "Error: Could not load program '%s'\n", config->szProgramPath);
        }
    }

    if (config->szDebuggerScript[0] != '\0') {
        Util_SafeStrCpy(g_state.sDebuggerScript, config->szDebuggerScript, PATH_MAX_LEN);
    }

    g_state.mode = MODE_RUNNING;
    g_state.restart = false;
    g_state.fullscreen = config->bFullscreen;

    return 0;
}

void AppController_Shutdown() {
    if (!s_initialized) return;
    
    Snapshot_Shutdown();
    Linapple_Shutdown();
    Logger::Destroy();
    
    s_initialized = false;
}

bool AppController_ShouldRestart() {
    return g_state.restart;
}

void AppController_SetRestart(bool restart) {
    g_state.restart = restart;
}
