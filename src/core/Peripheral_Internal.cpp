#include "Peripheral_Internal.h"
#include "LinAppleCore.h"

void Peripheral_Register_Internal() {
    // These will be enabled as peripherals are migrated in subsequent tasks.
    
#if defined(ENABLE_PERIPHERAL_SPEAKER)
    // Speaker is always present but we treat it as slot 0 for registration
    Peripheral_Register(&g_speaker_peripheral, 0);
#endif
    
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
    // Mockingboard is in slot 4 by default in many configs
    Peripheral_Register(&g_mockingboard_peripheral, 4);
#endif
}
