#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "core/LinAppleCore.h"
#include "core/Peripheral.h"
#include "core/Peripheral_Internal.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "core/Util_Text.h"
#include "core/Util_Path.h"
#include "apple2/DiskCommands.h"
#include "apple2/Memory.h"
#include "apple2/Disk.h"
#include "apple2/CPU.h"
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <climits>

namespace {
constexpr int SL6 = 6;
}

TEST_CASE("DiskIntegration: [PROT-01] Three-Layer Write Protection") {
    Linapple_Init();
    Peripheral_Manager_Init();
    Peripheral_Register_Internal();

    const char* fixture_woz = "../tests/fixtures/minimal.woz";
    const char* fixture_dsk = "../tests/fixtures/minimal.dsk";
    
    char f_user[PATH_MAX], f_os[PATH_MAX], f_format[PATH_MAX], f_rw[PATH_MAX];
    snprintf(f_user, sizeof(f_user), "%s/user_prot.dsk", get_current_dir_name());
    snprintf(f_os, sizeof(f_os), "%s/os_prot.dsk", get_current_dir_name());
    snprintf(f_format, sizeof(f_format), "%s/format_prot.woz", get_current_dir_name());
    snprintf(f_rw, sizeof(f_rw), "%s/rw.dsk", get_current_dir_name());

    auto copy_fix = [](const char* src_p, const char* dst_p, size_t size) {
      FilePtr src(fopen(src_p, "rb"), fclose);
      if (!src) {
          // Fallback if running from a different relative path
          std::string alt = "tests/fixtures/";
          alt += (strstr(src_p, ".woz") ? "minimal.woz" : "minimal.dsk");
          src.reset(fopen(alt.c_str(), "rb"));
      }
      FilePtr dst(fopen(dst_p, "wb"), fclose);
      REQUIRE(src != nullptr);
      std::vector<uint8_t> buf(size);
      fread(buf.data(), 1, size, src.get());
      fwrite(buf.data(), 1, size, dst.get());
    };
    
    copy_fix(fixture_dsk, f_user, 143360);
    copy_fix(fixture_dsk, f_os, 143360);
    copy_fix(fixture_woz, f_format, 1536);
    copy_fix(fixture_dsk, f_rw, 143360);

    DiskInsertCmd_t cmd{};
    cmd.drive = DISK_DRIVE_0;
    DiskStatus_t status{};
    size_t size = sizeof(status);
    
    // Layer 3: User Toggle
    Util_SafeStrCpy(cmd.path, f_user, DISK_INSERT_PATH_MAX);
    cmd.write_protected = true;
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    Peripheral_Manager_Think(0);
    Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    CHECK(status.drive0_write_protected == true);

    // Layer 2: OS Read-Only
    chmod(f_os, 0444); 
    Util_SafeStrCpy(cmd.path, f_os, DISK_INSERT_PATH_MAX);
    cmd.write_protected = false;
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    Peripheral_Manager_Think(0);
    Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    CHECK(status.drive0_write_protected == true); 
    
    // Layer 1: Format/Driver Capability (WOZ 2 currently has no write cap)
    Util_SafeStrCpy(cmd.path, f_format, DISK_INSERT_PATH_MAX);
    cmd.write_protected = false;
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    Peripheral_Manager_Think(0);
    Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    CHECK(status.drive0_write_protected == true); 

    // All clear: Writable
    Util_SafeStrCpy(cmd.path, f_rw, DISK_INSERT_PATH_MAX);
    cmd.write_protected = false;
    Peripheral_Command(SL6, DISK_CMD_INSERT, &cmd, sizeof(cmd));
    Peripheral_Manager_Think(0);
    Peripheral_Query(SL6, DISK_CMD_GET_STATUS, &status, &size);
    CHECK(status.drive0_write_protected == false);

    remove(f_user);
    remove(f_os);
    remove(f_format);
    remove(f_rw);
    Peripheral_Manager_Shutdown();
    Linapple_Shutdown();
}
