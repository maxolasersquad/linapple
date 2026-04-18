#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "core/ProgramLoader.h"
#include "core/LinAppleCore.h"
#include "apple2/CPU.h"
#include "apple2/Memory.h"
#include <cstdio>
#include <cstring>
#include <vector>

TEST_CASE("ProgramLoader: [PRG-01] Detection Failure on DSK") {
  const char* dsk_path = "test.dsk";
  FILE* f = fopen(dsk_path, "wb");
  uint8_t zero[1024];
  memset(zero, 0, sizeof(zero));
  for(int i=0; i<140; ++i) fwrite(zero, 1, 1024, f);
  fclose(f);

  CHECK(ProgramLoader_TryLoad(dsk_path) == PROGRAM_LOAD_NOT_A_PROGRAM);
  remove(dsk_path);
}

TEST_CASE("ProgramLoader: [PRG-02] Missing File") {
  CHECK(ProgramLoader_TryLoad("nonexistent.apl") == PROGRAM_LOAD_NOT_A_PROGRAM);
}

TEST_CASE("ProgramLoader: [PRG-03] Range Check Rejection") {
  const char* bad_apl = "bad.apl";
  FILE* f = fopen(bad_apl, "wb");
  uint16_t addr = 0xC000;
  uint16_t len = 0x0100;
  fwrite(&addr, 1, 2, f);
  fwrite(&len, 1, 2, f);
  uint8_t data[256] = {0};
  fwrite(data, 1, 256, f);
  fclose(f);

  CHECK(ProgramLoader_TryLoad(bad_apl) == PROGRAM_LOAD_INVALID);
  remove(bad_apl);
}

TEST_CASE("ProgramLoader: [PRG-04] APL Loading") {
  const char* test_apl = "test.apl";
  FILE* f = fopen(test_apl, "wb");
  uint16_t addr = 0x0800;
  uint16_t len = 0x0010;
  fwrite(&addr, 1, 2, f);
  fwrite(&len, 1, 2, f);
  uint8_t data[16];
  for(int i=0; i<16; ++i) data[i] = i + 1;
  fwrite(data, 1, 16, f);
  fclose(f);

  MemInitialize(); // Ensure memory is clean
  CHECK(ProgramLoader_TryLoad(test_apl) == PROGRAM_LOAD_OK);
  CHECK(regs.pc == 0x0800);
  CHECK(memcmp(mem + 0x0800, data, 16) == 0);

  remove(test_apl);
}

TEST_CASE("ProgramLoader: [PRG-05] PRG Loading (Word to Byte length)") {
  const char* test_prg = "test.prg";
  FILE* f = fopen(test_prg, "wb");
  uint32_t magic = 0x214C470A;
  uint8_t pad1 = 0;
  uint16_t addr = 0x1000;
  uint16_t word_len = 8; // 16 bytes
  fwrite(&magic, 1, 4, f);
  fwrite(&pad1, 1, 1, f);
  fwrite(&addr, 1, 2, f);
  fwrite(&word_len, 1, 2, f);
  uint8_t header_pad[128-9] = {0};
  fwrite(header_pad, 1, sizeof(header_pad), f);
  
  uint8_t data[16];
  for(int i=0; i<16; ++i) data[i] = 0xAA ^ i;
  fwrite(data, 1, 16, f);
  fclose(f);

  MemInitialize();
  CHECK(ProgramLoader_TryLoad(test_prg) == PROGRAM_LOAD_OK);
  CHECK(regs.pc == 0x1000);
  CHECK(memcmp(mem + 0x1000, data, 16) == 0);

  remove(test_prg);
}
