#pragma once

#include "frontends/common/AppConfig.h"

/**
 * High-level controller for LinApple lifecycle.
 * Manages the initialization and shutdown of all core subsystems.
 */

/**
 * Initialize all core subsystems based on the provided configuration.
 * Handles path resolution, registry, logger, and hardware startup.
 *
 * @return 0 on success, non-zero on error.
 */
int AppController_Initialize(AppConfig* config);

/**
 * Shut down all core subsystems in the correct order.
 */
void AppController_Shutdown();

/**
 * Check if the application has been marked for restart.
 */
bool AppController_ShouldRestart();

/**
 * Set or clear the restart flag.
 */
void AppController_SetRestart(bool restart);
