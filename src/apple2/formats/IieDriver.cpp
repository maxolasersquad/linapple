#include "apple2/formats/IieDriver.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "apple2/DiskGCR.h"

namespace {
constexpr char IIE_SIGNATURE[] = "SIMSYSTEM_IIE";
constexpr size_t IIE_SIGNATURE_LEN = 13;
constexpr int IIE_HEADER_SIZE = 88;
constexpr int IIE_TRACK_DATA_OFFSET = 30;
constexpr int SECTORS_PER_TRACK_16 = 16;
constexpr int DOS_TRACK_SIZE = 4096;
constexpr int IIE_TRACKS = 35;
constexpr int NIBBLES_PER_TRACK = 6656;

constexpr int IIE_VARIANT_OFFSET = 13;
constexpr int IIE_SECTOR_MAP_OFFSET = 14;
constexpr int IIE_NIBBLE_MAP_OFFSET = 14;

struct IieInstance {
  FILE* file = nullptr;
  uint8_t header[IIE_HEADER_SIZE]{};
  uint8_t sector_order[SECTORS_PER_TRACK_16]{};
  uint8_t work_buffer[GCR_WORKBUF_SIZE]{};
  bool os_readonly = false;

  IieInstance() = default;
  virtual ~IieInstance() {
    if (file) {
      fclose(file);
    }
  }

  IieInstance(const IieInstance&) = delete;
  IieInstance& operator=(const IieInstance&) = delete;
  IieInstance(IieInstance&&) = delete;
  IieInstance& operator=(IieInstance&&) = delete;
};

static void IieConvertSectorOrder(const uint8_t* sourceorder, uint8_t* sector_order) {
  for (int loop = 0; loop < SECTORS_PER_TRACK_16; ++loop) {
    uint8_t found = 0xFF;
    for (int loop2 = 0; loop2 < SECTORS_PER_TRACK_16; ++loop2) {
      if (sourceorder[loop2] == loop) {
        found = static_cast<uint8_t>(loop2);
        break;
      }
    }
    // Default to sector 0 if not found in map
    sector_order[loop] = (found == 0xFF) ? 0 : found;
  }
}
} // namespace

static DiskProbe_e IieProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)file_size;
  (void)ext_hint;

  if (header_size > IIE_VARIANT_OFFSET) {
    if (memcmp(header, IIE_SIGNATURE, IIE_SIGNATURE_LEN) == 0 &&
        header[IIE_VARIANT_OFFSET] <= 3) {
      return DISK_PROBE_DEFINITE;
    }
  }

  return DISK_PROBE_NO;
}

static DiskError_e IieOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  (void)file_offset;

  auto* instance = new IieInstance();
  instance->file = fopen(path, "r+b");
  if (instance->file != nullptr) {
    instance->os_readonly = false;
  } else {
    instance->file = fopen(path, "rb");
    if (instance->file != nullptr) {
      instance->os_readonly = true;
    } else {
      delete instance;
      return DISK_ERR_IO;
    }
  }

  if (out_os_readonly != nullptr) {
    *out_os_readonly = instance->os_readonly;
  }

  if (fread(instance->header, 1, IIE_HEADER_SIZE, instance->file) !=
      IIE_HEADER_SIZE) {
    fclose(instance->file);
    instance->file = nullptr;
    delete instance;
    return DISK_ERR_IO;
  }

  if (instance->header[IIE_VARIANT_OFFSET] <= 2) {
    IieConvertSectorOrder(&instance->header[IIE_SECTOR_MAP_OFFSET],
                          instance->sector_order);
  }

  *out_instance = reinterpret_cast<void*>(instance);
  return DISK_ERR_NONE;
}

static void IieClose(void* instance) {
  delete reinterpret_cast<IieInstance*>(instance);
}

static bool IieIsWriteProtected(void* instance) {
  (void)instance;
  return false;
}

static void IieReadTrack(void* instance, int track, int phase,
                         uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  auto* ii = reinterpret_cast<IieInstance*>(instance);

  if (track < 0 || track >= IIE_TRACKS) {
    *nibbles_out = 0;
    return;
  }

  if (ii->header[IIE_VARIANT_OFFSET] <= 2) {
    memset(ii->work_buffer, 0, GCR_WORKBUF_SIZE);
    fseek(ii->file,
          static_cast<long>(static_cast<size_t>(track) * DOS_TRACK_SIZE +
                            IIE_TRACK_DATA_OFFSET),
          SEEK_SET);
    fread(ii->work_buffer, 1, DOS_TRACK_SIZE, ii->file);
    *nibbles_out = static_cast<int>(GCR_NibblizeTrackCustomOrder(
        ii->work_buffer, trackImageBuffer, ii->sector_order, track));
  } else {
    // Pre-nibblized variant
    uint16_t nib_count = *reinterpret_cast<uint16_t*>(
        &ii->header[(track << 1) + IIE_NIBBLE_MAP_OFFSET]);

    if (nib_count > NIBBLES_PER_TRACK) {
      nib_count = NIBBLES_PER_TRACK;
    }

    uint32_t offset = IIE_HEADER_SIZE;
    for (int t = 0; t < track; ++t) {
      offset += *reinterpret_cast<uint16_t*>(
          &ii->header[(t << 1) + IIE_NIBBLE_MAP_OFFSET]);
    }
    fseek(ii->file, static_cast<long>(offset), SEEK_SET);
    *nibbles_out =
        static_cast<int>(fread(trackImageBuffer, 1, nib_count, ii->file));
  }
}

static void IieWriteTrack(void* instance, int track, int phase,
                          const uint8_t* trackImage, int nibbles) {
  (void)instance;
  (void)track;
  (void)phase;
  (void)trackImage;
  (void)nibbles;
  // Write is intentionally not implemented for IIE
}

extern "C" const DiskFormatDriver_t g_iie_driver = {
    LINAPPLE_DISK_ABI_VERSION,
    0,  // capabilities (no write)
    "IIE",
    nullptr,  // creatable_exts
    IieProbe,
    IieOpen,
    IieClose,
    IieIsWriteProtected,
    IieReadTrack,
    IieWriteTrack,
    nullptr,  // create
    nullptr   // read_flux_bit
};
