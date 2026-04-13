#ifndef LINAPPLE_PERIPHERAL_INTERNAL_H
#define LINAPPLE_PERIPHERAL_INTERNAL_H

#include "Peripheral.h"

// Forward declarations for peripherals before migration
// extern Peripheral_t g_ssc_peripheral;
// extern Peripheral_t g_mockingboard_peripheral;
// extern Peripheral_t g_speaker_peripheral;

/**
 * @brief Register all built-in peripherals with the manager.
 */
void Peripheral_Register_Internal();

#endif // LINAPPLE_PERIPHERAL_INTERNAL_H
