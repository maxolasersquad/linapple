#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "apple2/DiskCommands.h"
#include "apple2/DiskFormatDriver.h"
#include <cstddef>

static_assert(sizeof(DiskInsertCmd_t) == 512, "DiskInsertCmd_t must be exactly 512 bytes to fit one queue slot");

TEST_CASE("DiskABI: [DISK-01] DiskInsertCmd_t is exactly 512 bytes") {
  CHECK(sizeof(DiskInsertCmd_t) == 512);
}

TEST_CASE("DiskABI: [DISK-02] DiskInsertCmd_t field offsets are stable") {
  CHECK(offsetof(DiskInsertCmd_t, drive)                == 0);
  CHECK(offsetof(DiskInsertCmd_t, path)                 == 4);
  CHECK(offsetof(DiskInsertCmd_t, write_protected)      == 508);
  CHECK(offsetof(DiskInsertCmd_t, create_if_necessary)  == 509);
}

TEST_CASE("DiskABI: [DISK-03] Enum values match ABI specification") {
  CHECK(static_cast<int>(DISK_DRIVE_0)          == 0);
  CHECK(static_cast<int>(DISK_DRIVE_1)          == 1);
  CHECK(DISK_DEFAULT_SLOT                        == 6);
  CHECK(DISK_INSERT_PATH_MAX                     == 504);
  CHECK(DISK_STATUS_NAME_MAX                     == 64);
  CHECK(DISK_STATUS_PATH_MAX                     == 512);
  CHECK(static_cast<int>(LINAPPLE_DISK_ABI_VERSION) == 0);
}
