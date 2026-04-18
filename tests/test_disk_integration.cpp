#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "core/LinAppleCore.h"
#include "core/Peripheral.h"
#include "core/Peripheral_Internal.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "core/Util_Text.h"
#include "apple2/DiskCommands.h"
#include "apple2/Memory.h"
#include "apple2/Disk.h"
#include "apple2/CPU.h"
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>

namespace {
constexpr int SL6 = 6;
constexpr int CYCLES_PER_FRAME = 17030;
constexpr int MOTOR_ON_SWITCH = 0xC0E9;
constexpr int MOTOR_OFF_SWITCH = 0xC0E8;
constexpr int MOTOR_SPIN_DURATION = 2500000;
}

// Helper to run a few frames
void run_cycles(uint64_t cycles) {
    uint64_t count = 0;
    while(count < cycles) {
        uint32_t chunk = (cycles - count > static_cast<uint64_t>(CYCLES_PER_FRAME)) 
            ? static_cast<uint32_t>(CYCLES_PER_FRAME) 
            : static_cast<uint32_t>(cycles - count);
        Linapple_RunFrame(chunk);
        count += chunk;
    }
}

TEST_CASE("DiskIntegration: [INT-01] Startup Config Loading") {
    Linapple_Init();
    Configuration::Instance().SetString("Slots", "Disk Image 1", "../tests/fixtures/minimal.woz");
    
    Peripheral_Manager_Init();
    Peripheral_Register_Internal();
    
    DiskStatus_t status{};
    size_t size = sizeof(status);
    PeripheralStatus ps = Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    
    REQUIRE(ps == PERIPHERAL_OK);
    CHECK(status.drive0_loaded == true);
    CHECK(status.drive0_last_error == DISK_ERR_NONE);
    
    Peripheral_Manager_Shutdown();
    Linapple_Shutdown();
}

TEST_CASE("DiskIntegration: [INT-02] Missing Startup Image") {
    Linapple_Init();
    Configuration::Instance().SetString("Slots", "Disk Image 1", "nonexistent.dsk");
    
    Peripheral_Manager_Init();
    Peripheral_Register_Internal();
    
    DiskStatus_t status{};
    size_t size = sizeof(status);
    PeripheralStatus ps = Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    
    REQUIRE(ps == PERIPHERAL_OK);
    CHECK(status.drive0_loaded == false);
    CHECK(status.drive0_last_error == DISK_ERR_FILE_NOT_FOUND);
    
    Peripheral_Manager_Shutdown();
    Linapple_Shutdown();
}

TEST_CASE("DiskIntegration: [INT-03] Motor Activity Notification") {
    Linapple_Init();
    
    DiskInsertCmd_t cmd{};
    cmd.drive = DISK_DRIVE_0;
    Util_SafeStrCpy(cmd.path, "../tests/fixtures/minimal.woz", DISK_INSERT_PATH_MAX);
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    
    Peripheral_Manager_Think(0);
    
    CHECK(Peripheral_IsAnyActive() == false);
    
    IOMap_Dispatch(0, MOTOR_ON_SWITCH, 0, 0, 0); 
    run_cycles(100000);
    CHECK(Peripheral_IsAnyActive() == true);
    
    IOMap_Dispatch(0, MOTOR_OFF_SWITCH, 0, 0, 0);
    run_cycles(MOTOR_SPIN_DURATION); 
    CHECK(Peripheral_IsAnyActive() == false);
    
    Peripheral_Manager_Shutdown();
    Linapple_Shutdown();
}

TEST_CASE("DiskIntegration: [INT-04] WOZ Integration Check") {
    Linapple_Init();
    DiskInsertCmd_t cmd{};
    cmd.drive = DISK_DRIVE_0;
    Util_SafeStrCpy(cmd.path, "../tests/fixtures/minimal.woz", DISK_INSERT_PATH_MAX);
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    Peripheral_Manager_Think(0);
    
    DiskStatus_t status{};
    size_t size = sizeof(status);
    PeripheralStatus ps = Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    
    REQUIRE(ps == PERIPHERAL_OK);
    CHECK(status.drive0_loaded == true);
    CHECK(status.drive0_last_error == DISK_ERR_NONE);
    CHECK(strstr(status.drive0_full_path, "minimal.woz") != nullptr);
    
    Peripheral_Manager_Shutdown();
    Linapple_Shutdown();
}
