#include "Peripheral_Internal.h"
#include "LinAppleCore.h"

void Peripheral_Register_Internal(void) {
    // These will be enabled as peripherals are migrated in subsequent tasks.
    
    // Speaker is always present but we treat it as slot 0 for registration
    Peripheral_Register(&g_speaker_peripheral, 0);
    
    // Mockingboard is in slot 4 by default in many configs
    Peripheral_Register(&g_mockingboard_peripheral, 4);
}
