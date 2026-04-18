#include "core/ProgramLoader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "apple2/Memory.h"
#include "apple2/CPU.h"

namespace {
constexpr uint32_t PRG_MAGIC = 0x214C470A;
constexpr uint16_t IO_REGION_END = 0xCFFF;
constexpr uint32_t PRG_HEADER_SIZE = 128;
constexpr uint32_t APL_HEADER_SIZE = 4;
constexpr uint8_t MEM_FILL_VALUE = 0xFF;

enum class ProgramType {
  None,
  Prg,
  Apl
};

struct ProgramHeader {
  ProgramType type;
  uint16_t load_addr;
  uint16_t length;
  uint32_t offset;
};

static auto DetectProgram(const char* path, ProgramHeader* out_header)
    -> ProgramType {
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    return ProgramType::None;
  }

  fseek(f, 0, SEEK_END);
  uint32_t file_size = static_cast<uint32_t>(ftell(f));
  fseek(f, 0, SEEK_SET);

  uint8_t buffer[PRG_HEADER_SIZE];
  size_t read = fread(buffer, 1, sizeof(buffer), f);
  fclose(f);

  if (read < 8) {
    return ProgramType::None;
  }

  // PRG Check
  if (*reinterpret_cast<uint32_t*>(buffer) == PRG_MAGIC) {
    out_header->type = ProgramType::Prg;
    out_header->load_addr = *reinterpret_cast<uint16_t*>(buffer + 5);
    out_header->length =
        static_cast<uint16_t>(*reinterpret_cast<uint16_t*>(buffer + 7) << 1);
    out_header->offset = PRG_HEADER_SIZE;
    return ProgramType::Prg;
  }

  // APL Check (heuristic)
  uint16_t apl_len = *reinterpret_cast<uint16_t*>(buffer + 2);
  bool size_match =
      ((static_cast<uint32_t>(apl_len) + APL_HEADER_SIZE) == file_size) ||
      ((static_cast<uint32_t>(apl_len) + APL_HEADER_SIZE +
        ((256 - ((apl_len + APL_HEADER_SIZE) & 255)) & 255)) == file_size);

  if (size_match) {
    out_header->type = ProgramType::Apl;
    out_header->load_addr = *reinterpret_cast<uint16_t*>(buffer);
    out_header->length = apl_len;
    out_header->offset = APL_HEADER_SIZE;
    return ProgramType::Apl;
  }

  return ProgramType::None;
}
} // namespace

auto ProgramLoader_TryLoad(const char* path) -> ProgramLoadResult_e {
  ProgramHeader header = { ProgramType::None, 0, 0, 0 };
  ProgramType type = DetectProgram(path, &header);

  if (type == ProgramType::None) {
    return PROGRAM_LOAD_NOT_A_PROGRAM;
  }

  // Range check: reject I/O region $C000-$CFFF
  if (header.load_addr >= IO_REGION_START && header.load_addr <= IO_REGION_END) {
    return PROGRAM_LOAD_INVALID;
  }
  if (static_cast<uint32_t>(header.load_addr) + header.length > IO_REGION_START) {
    return PROGRAM_LOAD_INVALID;
  }

  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    return PROGRAM_LOAD_FILE_ERROR;
  }

  fseek(f, static_cast<int64_t>(header.offset), SEEK_SET);
  if (fread(mem + header.load_addr, 1, header.length, f) != header.length) {
    fclose(f);
    return PROGRAM_LOAD_FILE_ERROR;
  }
  fclose(f);

  // Set CPU state
  memset(memdirty, MEM_FILL_VALUE, NUM_PAGES_48K);
  regs.pc = header.load_addr;

  return PROGRAM_LOAD_OK;
}
