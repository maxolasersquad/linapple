#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstdio>
#include <cstring>
#include <vector>

#include "apple2/DiskGCR.h"
#include "apple2/formats/DoDriver.h"
#include "apple2/formats/PoDriver.h"
#include "doctest.h"

// Mock for enhancedisk
bool enhancedisk = true;

TEST_CASE("DiskDrivers: [DRV-01] DO Driver Probing") {
  std::vector<uint8_t> buffer(143360, 0);

  // Wrong size
  CHECK(g_do_driver.probe(buffer.data(), buffer.size(), 1000, ".dsk") ==
        DISK_PROBE_NO);

  // Correct size, ambiguous content
  CHECK(g_do_driver.probe(buffer.data(), 4096, 143360, ".dsk") ==
        DISK_PROBE_POSSIBLE);

  // Correct size, valid DOS VTOC
  // Track 17, Sector 0 is at 0x11000.
  // Bytes 0x11002-0x1100F should be 00 01 02 ... 0E
  for (int i = 0; i < 15; ++i) buffer[0x11000 + 2 + (i + 1) * 256] = i;
  // Wait, the logic in DoProbe is: loop=1..15, check VTOC_OFFSET + 2 + (loop *
  // PAGE_SIZE) Which is 0x11000 + 2 + (1*256), (2*256)...
  for (int loop = 1; loop <= 15; ++loop)
    buffer[0x11000 + 2 + (loop * 256)] = loop - 1;

  CHECK(g_do_driver.probe(buffer.data(), buffer.size(), 143360, ".dsk") ==
        DISK_PROBE_DEFINITE);
}

TEST_CASE("DiskDrivers: [DRV-02] PO Driver Probing") {
  std::vector<uint8_t> buffer(143360, 0);

  // Wrong size
  CHECK(g_po_driver.probe(buffer.data(), buffer.size(), 1000, ".po") ==
        DISK_PROBE_NO);

  // Correct size, valid ProDOS-like structure
  // ProDOS directory block 2 starts at 0x400.
  // prev at 0x400 + 0x100 = 0x500
  // next at 0x400 + 0x100 + 2 = 0x502
  buffer[0x500] = 0;
  buffer[0x501] = 0;  // prev = 0
  buffer[0x502] = 3;
  buffer[0x503] = 0;  // next = 3

  CHECK(g_po_driver.probe(buffer.data(), buffer.size(), 143360, ".po") ==
        DISK_PROBE_DEFINITE);
}

TEST_CASE("DiskDrivers: [DRV-03] DO Track Round-trip") {
  const char* tmp_file = "test_roundtrip.dsk";
  g_do_driver.create(tmp_file);

  void* instance = nullptr;
  REQUIRE(g_do_driver.open(tmp_file, 0, false, &instance) == DISK_ERR_NONE);

  uint8_t original_track[4096];
  for (int i = 0; i < 4096; ++i) original_track[i] = i & 0xFF;

  uint8_t nibble_buffer[0x2000];

  // We need a way to get data into the file first.
  // Since we don't have a "Write Sector" yet, we'll use a trick.
  // Actually, write_track works from a nibble buffer.

  // 1. Nibblize our test data
  uint8_t write_workbuf[GCR_WORKBUF_SIZE];
  memset(write_workbuf, 0, GCR_WORKBUF_SIZE);
  memcpy(write_workbuf, original_track, 4096);
  GCR_NibblizeTrack(write_workbuf, nibble_buffer, true, 0);

  // 2. Write it to disk
  g_do_driver.write_track(instance, 0, 0, nibble_buffer, 0x1A00);

  // 3. Read it back
  uint8_t read_nibbles[0x2000];
  int read_count = 0;
  enhancedisk = true;
  g_do_driver.read_track(instance, 0, 0, read_nibbles, &read_count);

  // 4. Denibblize the read data
  uint8_t read_workbuf[GCR_WORKBUF_SIZE];
  GCR_DenibblizeTrack(read_workbuf, read_nibbles, true, read_count);

  // 5. Compare
  CHECK(memcmp(original_track, read_workbuf, 4096) == 0);

  g_do_driver.close(instance);
  remove(tmp_file);
}
