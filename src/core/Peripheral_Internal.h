#ifndef LINAPPLE_PERIPHERAL_INTERNAL_H
#define LINAPPLE_PERIPHERAL_INTERNAL_H

#include "Peripheral.h"

// Forward declarations for migrated peripherals
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
extern Peripheral_t g_mockingboard_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_SPEAKER)
extern Peripheral_t g_speaker_peripheral;
#endif

/**
 * @brief Register all built-in peripherals with the manager.
 */
void Peripheral_Register_Internal(void);

#endif // LINAPPLE_PERIPHERAL_INTERNAL_H
