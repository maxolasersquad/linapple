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

#if defined(ENABLE_PERIPHERAL_DISK)
extern Peripheral_t g_disk_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_SSC)
extern Peripheral_t g_ssc_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_PRINTER)
extern Peripheral_t g_printer_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_HARDDISK)
extern Peripheral_t g_harddisk_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_MOUSE)
extern Peripheral_t g_mouse_peripheral;
#endif

#if defined(ENABLE_PERIPHERAL_CLOCK)
extern Peripheral_t g_clock_peripheral;
#endif

/**
 * @brief Register all built-in peripherals with the manager.
 */
void Peripheral_Register_Internal(void);

#endif // LINAPPLE_PERIPHERAL_INTERNAL_H
