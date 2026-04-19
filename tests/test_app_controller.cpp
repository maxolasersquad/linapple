#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "frontends/common/AppController.h"
#include "core/Common_Globals.h"

TEST_CASE("AppController: Initialize and Shutdown") {
    AppConfig config = {};
    AppConfig_Default(&config);
    
    // Test initialization
    int result = AppController_Initialize(&config);
    CHECK(result == 0);
    CHECK(g_state.mode == MODE_RUNNING);
    
    // Test shutdown
    AppController_Shutdown();
}
