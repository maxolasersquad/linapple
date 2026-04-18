#include "apple2/formats/PoDriver.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "apple2/DiskGCR.h"
#include "core/Util_Path.h"
#include "core/Util_Text.h"

// TODO: pass via driver config, not global
extern bool enhancedisk;

// TODO: share between DO/PO
namespace {
constexpr int DISK_SIZE_140K = 143360;
constexpr int MIN_140K_DISK_SIZE = 143105;
constexpr int MAX_140K_DISK_SIZE = 143364;
constexpr int DISK_SIZE_140K_ALT2 = 143488;
constexpr int DOS_TRACK_SIZE = 4096;
constexpr int VTOC_OFFSET = 0x11000;
constexpr int PAGE_SIZE = 0x0100;
constexpr int PRODOS_BLOCK_SIZE = 512;

struct PoInstance {
  FILE* file = nullptr;
  uint32_t macbinary_offset = 0;
  bool os_readonly = false;
  uint8_t work_buffer[GCR_WORKBUF_SIZE]{};

  PoInstance() = default;
  ~PoInstance() {
    if (file) {
      fclose(file);
    }
  }
};
}  // namespace

static DiskProbe_e PoProbe(const uint8_t* header, size_t header_size,
                           uint32_t file_size, const char* ext_hint) {
  (void)header;
  (void)header_size;
  (void)ext_hint;
  // PO probe accepts DISK_SIZE_140K_ALT2 but NOT DISK_SIZE_140K_ALT1
  if (file_size < MIN_140K_DISK_SIZE || file_size > MAX_140K_DISK_SIZE) {
    if (file_size != DISK_SIZE_140K_ALT2) {
      return DISK_PROBE_NO;
    }
  }

  // ProDOS directory chain check on track 0, starting at block 2 (Sectors 4,5
  // of Track 0). A typical ProDOS directory header at block 2, sector 4 (0x400)
  // has next/prev pointers.
  if (header_size >= (2 * PRODOS_BLOCK_SIZE) + PAGE_SIZE + 2) {
    uint16_t prev = *reinterpret_cast<const uint16_t*>(
        header + (2 * PRODOS_BLOCK_SIZE) + PAGE_SIZE);
    uint16_t next = *reinterpret_cast<const uint16_t*>(
        header + (2 * PRODOS_BLOCK_SIZE) + PAGE_SIZE + 2);
    // In ProDOS directory block 2, prev is always 0.
    if (prev == 0 && next > 2 && next < 35 * 16 / 2) {
      return DISK_PROBE_DEFINITE;
    }
  }

  if (header_size >= VTOC_OFFSET + 2 + (13 * PAGE_SIZE)) {
    bool mismatch = false;
    for (int loop = 5; loop <= 13; ++loop) {
      if (header[VTOC_OFFSET + 2 + (loop * PAGE_SIZE)] != 14 - loop) {
        mismatch = true;
        break;
      }
    }
    if (!mismatch) {
      return DISK_PROBE_DEFINITE;
    }
  }

  if (strcmp(ext_hint, ".po") == 0) {
    return DISK_PROBE_POSSIBLE;
  }

  return DISK_PROBE_POSSIBLE;  // Correct size is often enough to be "possible"
}

static DiskError_e PoOpen(const char* path, uint32_t file_offset,
                          bool os_readonly, void** out_instance) {
  auto* instance = new PoInstance();
  instance->file = fopen(path, os_readonly ? "rb" : "r+b");
  if (!instance->file) {
    delete instance;
    return DISK_ERR_IO;
  }
  instance->macbinary_offset = file_offset;
  instance->os_readonly = os_readonly;
  *out_instance = reinterpret_cast<void*>(instance);
  return DISK_ERR_NONE;
}

static void PoClose(void* instance) {
  delete reinterpret_cast<PoInstance*>(instance);
}

static bool PoIsWriteProtected(void* instance) {
  (void)instance;
  return false;
}

static void PoReadTrack(void* instance, int track, int phase,
                        uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  auto* di = reinterpret_cast<PoInstance*>(instance);
  memset(di->work_buffer, 0, GCR_WORKBUF_SIZE);
  fseek(di->file,
        static_cast<long>(di->macbinary_offset + (track * DOS_TRACK_SIZE)),
        SEEK_SET);
  fread(di->work_buffer, 1, DOS_TRACK_SIZE, di->file);

  uint32_t nibbles =
      GCR_NibblizeTrack(di->work_buffer, trackImageBuffer, false, track);
  *nibbles_out = static_cast<int>(nibbles);

  if (!enhancedisk) {
    GCR_SkewTrack(di->work_buffer, track, *nibbles_out, trackImageBuffer);
  }
}

static void PoWriteTrack(void* instance, int track, int phase,
                         const uint8_t* trackImage, int nibbles) {
  (void)phase;
  auto* di = reinterpret_cast<PoInstance*>(instance);
  if (di->os_readonly) return;

  memset(di->work_buffer, 0, GCR_WORKBUF_SIZE);
  GCR_DenibblizeTrack(di->work_buffer, const_cast<uint8_t*>(trackImage), false,
                      nibbles);
  fseek(di->file,
        static_cast<long>(di->macbinary_offset + (track * DOS_TRACK_SIZE)),
        SEEK_SET);
  fwrite(di->work_buffer, 1, DOS_TRACK_SIZE, di->file);
}

static DiskError_e PoCreate(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return DISK_ERR_IO;

  uint8_t zero[1024];
  memset(zero, 0, sizeof(zero));
  for (int i = 0; i < DISK_SIZE_140K / 1024; ++i) {
    fwrite(zero, 1, 1024, f);
  }
  fclose(f);
  return DISK_ERR_NONE;
}

static const char* const g_po_creatable_exts[] = {".po", nullptr};

extern "C" const DiskFormatDriver_t g_po_driver = {
    LINAPPLE_DISK_ABI_VERSION,
    DRIVER_CAP_WRITE,
    "ProDOS Order",
    g_po_creatable_exts,
    PoProbe,
    PoOpen,
    PoClose,
    PoIsWriteProtected,
    PoReadTrack,
    PoWriteTrack,
    PoCreate,
    nullptr  // read_flux_bit
};
