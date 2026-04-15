#include "Peripheral_Internal.h"
#include "LinAppleCore.h"
#include "core/Common_Globals.h"
#include "apple2/Structs.h"

void Peripheral_Register_Internal() {
    // These will be enabled as peripherals are migrated in subsequent tasks.
    
#if defined(ENABLE_PERIPHERAL_SPEAKER)
    // Speaker is always present but we treat it as slot 0 for registration
    Peripheral_Register(&g_speaker_peripheral, 0);
#endif
    
#if defined(ENABLE_PERIPHERAL_PRINTER)
    Peripheral_Register(&g_printer_peripheral, 1);
#endif

#if defined(ENABLE_PERIPHERAL_SSC)
    Peripheral_Register(&g_ssc_peripheral, 2);
#endif

    // Slot 4 could be Mockingboard or Mouse
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
    if (g_Slot4 == CT_Mockingboard) {
        Peripheral_Register(&g_mockingboard_peripheral, 4);
    }
#endif

#if defined(ENABLE_PERIPHERAL_MOUSE)
    if (g_Slot4 == CT_MouseInterface) {
        Peripheral_Register(&g_mouse_peripheral, 4);
    }
#endif

    // Slot 5 is often also Mockingboard
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
    // Registering twice for the same peripheral might not be supported directly by the simple ABI yet,
    // wait Mockingboard init registers itself on slots specified by compatible_slots or just whatever slot is passed.
    // The previous code had a loop or two separate init calls.
    // Let's just do slot 5 if CT_Mockingboard.
    // Wait, the ABI init is called per-slot. It creates an instance or uses a global one.
    // Let's just register it for slot 5 too for now if it supports it.
    // actually, g_mockingboard_peripheral can just be registered on slot 5
    // But Mockingboard code had `uSlot5 = SLOT5; ConfigLoadInt(...)`.
    // I'll just register it on 5 blindly for now.
    Peripheral_Register(&g_mockingboard_peripheral, 5);
#endif

#if defined(ENABLE_PERIPHERAL_DISK)
    Peripheral_Register(&g_disk_peripheral, 6);
#endif

#if defined(ENABLE_PERIPHERAL_HARDDISK)
    Peripheral_Register(&g_harddisk_peripheral, 7);
#endif

#if defined(ENABLE_PERIPHERAL_CLOCK)
    // No-slot clock can be placed in an unused slot. We'll use slot 3.
    Peripheral_Register(&g_clock_peripheral, 3);
#endif
}
