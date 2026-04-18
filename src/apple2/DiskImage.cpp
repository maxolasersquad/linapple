/*
linapple : An Apple //e emulator for Linux

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "apple2/DiskImage.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "apple2/CPU.h"
#include "apple2/Disk.h"
#include "apple2/DiskGCR.h"
#include "apple2/DiskLoader.h"
#include "apple2/Memory.h"
#include "apple2/Structs.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/Log.h"
#include "core/Util_Path.h"
#include "core/Util_Text.h"

using imageinforec = struct imageinforec {
  char filename[PATH_MAX_LEN]{};
  DiskFormatDriver_t* driver = nullptr;
  FilePtr file;
  uint32_t offset = 0;
  bool writeProtected = false;
  std::unique_ptr<uint8_t[], void (*)(void*)> header;
  bool validTrack[TRACKS]{};

  imageinforec() : file(nullptr, fclose), header(nullptr, free) {}
};
using imageinfoptr = imageinforec*;

static auto Util_GetFileSize(FILE* f) -> size_t {
  long current = ftell(f);
  fseek(f, 0, SEEK_END);
  size_t size = static_cast<size_t>(ftell(f));
  fseek(f, current, SEEK_SET);
  return size;
}

static DiskError_e GenericOpen(const char* path, uint32_t file_offset,
                               bool* out_os_readonly, void** out_instance,
                               DiskFormatDriver_t* driver) {
  auto ptr = std::unique_ptr<imageinforec>(new imageinforec());
  if (!ptr) return DISK_ERR_OUT_OF_MEMORY;

  ptr->file.reset(fopen(path, "r+b"));
  if (ptr->file) {
    ptr->writeProtected = false;
  } else {
    ptr->file.reset(fopen(path, "rb"));
    if (ptr->file) {
      ptr->writeProtected = true;
    } else {
      return DISK_ERR_IO;
    }
  }

  if (out_os_readonly) *out_os_readonly = ptr->writeProtected;

  Util_SafeStrCpy(ptr->filename, path, PATH_MAX_LEN);
  ptr->offset = file_offset;
  ptr->driver = driver;

  size_t size = Util_GetFileSize(ptr->file.get());
  for (bool& track : ptr->validTrack) {
    track = (size > file_offset);
  }

  *out_instance = ptr.release();
  return DISK_ERR_NONE;
}

static void GenericClose(void* instance) {
  delete reinterpret_cast<imageinfoptr>(instance);
}

static bool GenericIsWriteProtected(void* instance) {
  return reinterpret_cast<imageinfoptr>(instance)->writeProtected;
}

static DiskProbe_e AplProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  if (header_size < 4) return DISK_PROBE_NO;
  uint32_t length = *reinterpret_cast<const uint16_t*>(header + 2);
  bool size_match =
      (((length + 4) == file_size) ||
       ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == file_size));
  if (size_match) return DISK_PROBE_DEFINITE;
  if (strcmp(ext_hint, ".apl") == 0) return DISK_PROBE_POSSIBLE;
  return DISK_PROBE_NO;
}

static DiskError_e AplOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_apl);
}

static DiskProbe_e PrgProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)file_size;
  if (header_size < 4) return DISK_PROBE_NO;
  if (*reinterpret_cast<const uint32_t*>(header) == 0x214C470A)
    return DISK_PROBE_DEFINITE;
  if (strcmp(ext_hint, ".prg") == 0) return DISK_PROBE_POSSIBLE;
  return DISK_PROBE_NO;
}

static DiskError_e PrgOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_prg);
}

// --- DO Format ---
static DiskProbe_e DoProbe(const uint8_t* header, size_t header_size,
                           uint32_t file_size, const char* ext_hint) {
  if (file_size < MIN_140K_DISK_SIZE || file_size > MAX_140K_DISK_SIZE) {
    if (file_size != DISK_SIZE_140K_ALT1 && file_size != DISK_SIZE_140K_ALT2)
      return DISK_PROBE_NO;
  }
  if (strcmp(ext_hint, ".nib") == 0 || strcmp(ext_hint, ".po") == 0)
    return DISK_PROBE_NO;

  if (header_size >= VTOC_OFFSET + 2 + (15 * PAGE_SIZE)) {
    bool mismatch = false;
    for (int loop = 1; loop <= 15; ++loop) {
      if (header[VTOC_OFFSET + 2 + (loop * PAGE_SIZE)] != loop - 1) {
        mismatch = true;
        break;
      }
    }
    if (!mismatch) return DISK_PROBE_DEFINITE;
  }
  return DISK_PROBE_POSSIBLE;
}

static DiskError_e DoOpen(const char* path, uint32_t file_offset,
                          bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_do);
}

static void DoReadTrack(void* instance, int track, int phase,
                        uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  uint8_t workbuf[GCR_WORKBUF_SIZE] = {};
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fread(workbuf, 1, DOS_TRACK_SIZE, ptr->file.get());
  *nibbles_out = static_cast<int>(
      GCR_NibblizeTrack(workbuf, trackImageBuffer, true, track));
  if (!enhancedisk) {
    GCR_SkewTrack(workbuf, track, *nibbles_out, trackImageBuffer);
  }
}

static void DoWriteTrack(void* instance, int track, int phase,
                         const uint8_t* trackImage, int nibbles) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  uint8_t workbuf[GCR_WORKBUF_SIZE] = {};
  GCR_DenibblizeTrack(workbuf, const_cast<uint8_t*>(trackImage), true, nibbles);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fwrite(workbuf, 1, DOS_TRACK_SIZE, ptr->file.get());
}

// --- PO Format ---
static DiskProbe_e PoProbe(const uint8_t* header, size_t header_size,
                           uint32_t file_size, const char* ext_hint) {
  if (file_size < MIN_140K_DISK_SIZE || file_size > MAX_140K_DISK_SIZE) {
    if (file_size != DISK_SIZE_140K_ALT2) return DISK_PROBE_NO;
  }
  if (strcmp(ext_hint, ".nib") == 0 || strcmp(ext_hint, ".do") == 0 ||
      strcmp(ext_hint, ".dsk") == 0)
    return DISK_PROBE_NO;

  if (header_size >= VTOC_OFFSET + 2 + (13 * PAGE_SIZE)) {
    bool mismatch = false;
    for (int loop = 5; loop <= 13; ++loop) {
      if (header[VTOC_OFFSET + 2 + (loop * PAGE_SIZE)] != 14 - loop) {
        mismatch = true;
        break;
      }
    }
    if (!mismatch) return DISK_PROBE_DEFINITE;
  }
  return DISK_PROBE_POSSIBLE;
}

static DiskError_e PoOpen(const char* path, uint32_t file_offset,
                          bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_po);
}

static void PoReadTrack(void* instance, int track, int phase,
                        uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  uint8_t workbuf[GCR_WORKBUF_SIZE] = {};
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fread(workbuf, 1, DOS_TRACK_SIZE, ptr->file.get());
  *nibbles_out = static_cast<int>(
      GCR_NibblizeTrack(workbuf, trackImageBuffer, false, track));
  if (!enhancedisk) {
    GCR_SkewTrack(workbuf, track, *nibbles_out, trackImageBuffer);
  }
}

static void PoWriteTrack(void* instance, int track, int phase,
                         const uint8_t* trackImage, int nibbles) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  uint8_t workbuf[GCR_WORKBUF_SIZE] = {};
  GCR_DenibblizeTrack(workbuf, const_cast<uint8_t*>(trackImage), false,
                      nibbles);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fwrite(workbuf, 1, DOS_TRACK_SIZE, ptr->file.get());
}

// --- NIB Format ---
static DiskProbe_e NibProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)header;
  (void)header_size;
  if (file_size == DISK_SIZE_NIB) return DISK_PROBE_DEFINITE;
  if (strcmp(ext_hint, ".nib") == 0) return DISK_PROBE_POSSIBLE;
  return DISK_PROBE_NO;
}

static DiskError_e NibOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_nib);
}

static void NibReadTrack(void* instance, int track, int phase,
                         uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset +
                          static_cast<uint32_t>(track * NIBBLES_PER_TRACK)),
        SEEK_SET);
  *nibbles_out = static_cast<int>(
      fread(trackImageBuffer, 1, NIBBLES_PER_TRACK, ptr->file.get()));
}

static void NibWriteTrack(void* instance, int track, int phase,
                          const uint8_t* trackImage, int nibbles) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset +
                          static_cast<uint32_t>(track * NIBBLES_PER_TRACK)),
        SEEK_SET);
  fwrite(trackImage, 1, static_cast<size_t>(nibbles), ptr->file.get());
}

// --- NB2 Format ---
static DiskProbe_e Nb2Probe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)header;
  (void)header_size;
  (void)ext_hint;
  if (file_size == DISK_SIZE_NB2) return DISK_PROBE_DEFINITE;
  return DISK_PROBE_NO;
}

static DiskError_e Nb2Open(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  return GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_nb2);
}

static void Nb2ReadTrack(void* instance, int track, int phase,
                         uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset +
                          static_cast<uint32_t>(track * NIBBLES_PER_TRACK_NB2)),
        SEEK_SET);
  *nibbles_out = static_cast<int>(
      fread(trackImageBuffer, 1, NIBBLES_PER_TRACK_NB2, ptr->file.get()));
}

static void Nb2WriteTrack(void* instance, int track, int phase,
                          const uint8_t* trackImage, int nibbles) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  fseek(ptr->file.get(),
        static_cast<long>(ptr->offset +
                          static_cast<uint32_t>(track * NIBBLES_PER_TRACK_NB2)),
        SEEK_SET);
  fwrite(trackImage, 1, static_cast<size_t>(nibbles), ptr->file.get());
}

// --- IIE Format ---
const int IIE_HEADER_SIZE = 88;
const int IIE_TRACK_DATA_OFFSET = 30;
const char IIE_SIGNATURE[] = "SIMSYSTEM_IIE";
const int IIE_SIGNATURE_LEN = 13;

static DiskProbe_e IieProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)file_size;
  (void)ext_hint;
  if (header_size < IIE_SIGNATURE_LEN) return DISK_PROBE_NO;
  if (strncmp(reinterpret_cast<const char*>(header), IIE_SIGNATURE,
              IIE_SIGNATURE_LEN) == 0 &&
      header[IIE_SIGNATURE_LEN] <= 3) {
    return DISK_PROBE_DEFINITE;
  }
  return DISK_PROBE_NO;
}

static void IieConvertSectorOrder(const uint8_t* sourceorder,
                                  uint8_t* sector_order) {
  for (int loop = 0; loop < SECTORS_PER_TRACK_16; ++loop) {
    uint8_t found = 0xFF;
    for (int loop2 = 0; loop2 < SECTORS_PER_TRACK_16; ++loop2) {
      if (sourceorder[loop2] == loop) {
        found = static_cast<uint8_t>(loop2);
        break;
      }
    }
    sector_order[loop] = (found == 0xFF) ? 0 : found;
  }
}

static DiskError_e IieOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  DiskError_e err =
      GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_iie);
  if (err != DISK_ERR_NONE) return err;

  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(*out_instance);
  ptr->header.reset(static_cast<uint8_t*>(malloc(IIE_HEADER_SIZE)));
  if (!ptr->header) return DISK_ERR_OUT_OF_MEMORY;
  fseek(ptr->file.get(), static_cast<long>(file_offset), SEEK_SET);
  fread(ptr->header.get(), 1, IIE_HEADER_SIZE, ptr->file.get());
  return DISK_ERR_NONE;
}

static void IieReadTrack(void* instance, int track, int phase,
                         uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  if (ptr->header[IIE_SIGNATURE_LEN] <= 2) {
    uint8_t workbuf[GCR_WORKBUF_SIZE] = {};
    uint8_t iie_sector_order[SECTORS_PER_TRACK_16];
    IieConvertSectorOrder(&ptr->header[14], iie_sector_order);
    fseek(ptr->file.get(),
          static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE) +
                            IIE_TRACK_DATA_OFFSET),
          SEEK_SET);
    fread(workbuf, 1, DOS_TRACK_SIZE, ptr->file.get());
    *nibbles_out = static_cast<int>(GCR_NibblizeTrackCustomOrder(
        workbuf, trackImageBuffer, iie_sector_order, track));
  } else {
    *nibbles_out =
        *reinterpret_cast<uint16_t*>(&ptr->header[(track << 1) + 14]);
    uint32_t offset = ptr->offset + IIE_HEADER_SIZE;
    for (int t = 0; t < track; ++t)
      offset += *reinterpret_cast<uint16_t*>(&ptr->header[(t << 1) + 14]);
    fseek(ptr->file.get(), static_cast<long>(offset), SEEK_SET);
    fread(trackImageBuffer, 1, static_cast<size_t>(*nibbles_out),
          ptr->file.get());
  }
}

const int WOZ2_SIGNATURE_LEN = 8;
const int WOZ2_QUARTER_TRACKS_PER_TRACK = 4;
const uint8_t WOZ2_UNRECORDED_TRACK = 0xFF;
const int WOZ2_SYNC_BYTES_16SECTOR = 4;
const int WOZ2_TRAILING_ZEROS_16SECTOR = 2;
const int WOZ2_SYNC_BYTES_13SECTOR = 7;

static DiskProbe_e WozProbe(const uint8_t* header, size_t header_size,
                            uint32_t file_size, const char* ext_hint) {
  (void)ext_hint;
  if (header_size < WOZ2_SIGNATURE_LEN) return DISK_PROBE_NO;
  if (strncmp(reinterpret_cast<const char*>(header), "WOZ2\xFF\n\r\n",
              WOZ2_SIGNATURE_LEN) == 0 &&
      file_size >= WOZ2_HEADER_SIZE) {
    return DISK_PROBE_DEFINITE;
  }
  return DISK_PROBE_NO;
}

static DiskError_e WozOpen(const char* path, uint32_t file_offset,
                           bool* out_os_readonly, void** out_instance) {
  DiskError_e err =
      GenericOpen(path, file_offset, out_os_readonly, out_instance, &g_driver_woz);
  if (err != DISK_ERR_NONE) return err;

  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(*out_instance);
  ptr->header.reset(static_cast<uint8_t*>(malloc(WOZ2_HEADER_SIZE)));
  if (!ptr->header) return DISK_ERR_OUT_OF_MEMORY;
  fseek(ptr->file.get(), static_cast<long>(file_offset), SEEK_SET);
  fread(ptr->header.get(), 1, WOZ2_HEADER_SIZE, ptr->file.get());
  return DISK_ERR_NONE;
}

static uint32_t woz2_scan_sync_bytes(const uint8_t* buffer, uint32_t bit_count,
                                     uint32_t sync_bytes_needed,
                                     uint32_t trailing_0_count) {
#define FETCH_BIT(idx) ((buffer[(idx) / 8] & (0x80 >> ((idx) % 8))) ? 1 : 0)
  uint32_t i = 0;
  while (i < bit_count) {
    while (i < bit_count && FETCH_BIT(i) == 0) ++i;
    if (i >= bit_count) break;
    uint32_t nr_FFs = 0;
    uint32_t start_i = i;
    while (i < bit_count) {
      uint32_t nr_1s = 0;
      while (i < bit_count && FETCH_BIT(i) == 1) {
        ++nr_1s;
        ++i;
      }
      if (nr_1s < 8) break;
      if (nr_1s > 8) nr_FFs = 1;
      uint32_t nr_0s = 0;
      while (i < bit_count && FETCH_BIT(i) == 0) {
        ++nr_0s;
        ++i;
      }
      if (nr_0s != trailing_0_count) break;
      if (++nr_FFs == sync_bytes_needed) return i;
    }
    i = start_i + 1;
  }
  return 0;
#undef FETCH_BIT
}

static void WozReadTrack(void* instance, int track, int phase,
                         uint8_t* trackImageBuffer, int* nibbles_out) {
  (void)phase;
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(instance);
  uint8_t* tmap = ptr->header.get() + WOZ2_TMAP_OFFSET;
  uint8_t* trks = ptr->header.get() + WOZ2_TRKS_OFFSET;
  uint32_t tmap_index =
      WOZ2_QUARTER_TRACKS_PER_TRACK * static_cast<uint32_t>(track);
  uint32_t trks_index = tmap[tmap_index];

  if (trks_index == WOZ2_UNRECORDED_TRACK) {
    for (uint32_t i = 0; i < NIBBLES_PER_TRACK; ++i)
      trackImageBuffer[i] = static_cast<uint8_t>(rand() & 0xFF);
    *nibbles_out = static_cast<int>(NIBBLES_PER_TRACK);
    return;
  }

  uint8_t* trk = trks + (trks_index * WOZ2_TRK_SIZE);
  uint16_t starting_block = trk[0] | (trk[1] << 8);
  uint16_t block_count = trk[2] | (trk[3] << 8);
  uint32_t bit_count = trk[4] | (trk[5] << 8) | (trk[6] << 16) | (trk[7] << 24);

  fseek(
      ptr->file.get(),
      static_cast<long>(ptr->offset + (starting_block * WOZ2_DATA_BLOCK_SIZE)),
      SEEK_SET);
  uint32_t byte_count = block_count * WOZ2_DATA_BLOCK_SIZE;
  std::vector<uint8_t> buffer(byte_count);
  fread(buffer.data(), 1, byte_count, ptr->file.get());

  uint32_t i =
      woz2_scan_sync_bytes(buffer.data(), bit_count, WOZ2_SYNC_BYTES_16SECTOR,
                           WOZ2_TRAILING_ZEROS_16SECTOR);
  if (i == 0)
    i = woz2_scan_sync_bytes(buffer.data(), bit_count, WOZ2_SYNC_BYTES_13SECTOR,
                             0);
  if (i == 0 || bit_count == 0) {
    *nibbles_out = 0;
    return;
  }

#define FETCH_BIT(idx) \
  ((buffer[(idx % bit_count) / 8] & (0x80 >> ((idx % bit_count) % 8))) ? 1 : 0)
  uint32_t bits_left = bit_count;
  uint32_t nibbles_done = 0;
  while (nibbles_done < NIBBLES_PER_TRACK && bits_left >= 8) {
    while (bits_left > 0 && FETCH_BIT(i) == 0) {
      ++i;
      --bits_left;
    }
    if (bits_left < 8) break;
    uint8_t nibble = 0;
    for (int b = 0; b < 8; ++b) {
      nibble = static_cast<uint8_t>((nibble << 1) | FETCH_BIT(i));
      ++i;
    }
    bits_left -= 8;
    trackImageBuffer[nibbles_done++] = nibble;
  }
  *nibbles_out = static_cast<int>(nibbles_done);
#undef FETCH_BIT
}

DiskFormatDriver_t g_driver_woz = {LINAPPLE_DISK_ABI_VERSION,
                                   0,
                                   "WOZ2",
                                   nullptr,
                                   WozProbe,
                                   WozOpen,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   WozReadTrack,
                                   nullptr,
                                   nullptr,
                                   nullptr};
DiskFormatDriver_t g_driver_iie = {LINAPPLE_DISK_ABI_VERSION,
                                   0,
                                   "IIE",
                                   nullptr,
                                   IieProbe,
                                   IieOpen,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   IieReadTrack,
                                   nullptr,
                                   nullptr,
                                   nullptr};
DiskFormatDriver_t g_driver_nib = {LINAPPLE_DISK_ABI_VERSION,
                                   DRIVER_CAP_WRITE,
                                   "NIB",
                                   nullptr,
                                   NibProbe,
                                   NibOpen,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   NibReadTrack,
                                   NibWriteTrack,
                                   nullptr,
                                   nullptr};
DiskFormatDriver_t g_driver_nb2 = {LINAPPLE_DISK_ABI_VERSION,
                                   DRIVER_CAP_WRITE,
                                   "NB2",
                                   nullptr,
                                   Nb2Probe,
                                   Nb2Open,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   Nb2ReadTrack,
                                   Nb2WriteTrack,
                                   nullptr,
                                   nullptr};
DiskFormatDriver_t g_driver_do = {LINAPPLE_DISK_ABI_VERSION,
                                  DRIVER_CAP_WRITE,
                                  "DO",
                                  nullptr,
                                  DoProbe,
                                  DoOpen,
                                  GenericClose,
                                  GenericIsWriteProtected,
                                  DoReadTrack,
                                  DoWriteTrack,
                                  nullptr,
                                  nullptr};
DiskFormatDriver_t g_driver_po = {LINAPPLE_DISK_ABI_VERSION,
                                  DRIVER_CAP_WRITE,
                                  "PO",
                                  nullptr,
                                  PoProbe,
                                  PoOpen,
                                  GenericClose,
                                  GenericIsWriteProtected,
                                  PoReadTrack,
                                  PoWriteTrack,
                                  nullptr,
                                  nullptr};
DiskFormatDriver_t g_driver_prg = {LINAPPLE_DISK_ABI_VERSION,
                                   0,
                                   "PRG",
                                   nullptr,
                                   PrgProbe,
                                   PrgOpen,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   nullptr};
DiskFormatDriver_t g_driver_apl = {LINAPPLE_DISK_ABI_VERSION,
                                   0,
                                   "APL",
                                   nullptr,
                                   AplProbe,
                                   AplOpen,
                                   GenericClose,
                                   GenericIsWriteProtected,
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   nullptr};

bool ImageBoot(DiskImagePtr_t imageHandle) {
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  if (ptr->driver == &g_driver_apl) {
    fseek(ptr->file.get(), static_cast<long>(ptr->offset), SEEK_SET);
    uint16_t addr = 0;
    uint16_t len = 0;
    fread(&addr, 1, 2, ptr->file.get());
    fread(&len, 1, 2, ptr->file.get());
    if (addr + len > 0xC000) return false;
    fread(mem + addr, 1, len, ptr->file.get());
    memset(memdirty, 0xFF, NUM_PAGES_48K);
    regs.pc = addr;
    ptr->writeProtected = true;
    return true;
  } else if (ptr->driver == &g_driver_prg) {
    fseek(ptr->file.get(), static_cast<long>(ptr->offset + 5), SEEK_SET);
    uint16_t addr = 0;
    uint16_t len = 0;
    fread(&addr, 1, 2, ptr->file.get());
    fread(&len, 1, 2, ptr->file.get());
    len <<= 1;
    if (addr + len > 0xC000) return false;
    fseek(ptr->file.get(), static_cast<long>(ptr->offset + 128), SEEK_SET);
    fread(mem + addr, 1, len, ptr->file.get());
    memset(memdirty, 0xFF, NUM_PAGES_48K);
    regs.pc = addr;
    ptr->writeProtected = true;
    return true;
  }
  return false;
}

void ImageClose(DiskImagePtr_t imageHandle) {
  if (imageHandle) {
    imageinfoptr ptr = reinterpret_cast<imageinfoptr>(imageHandle);
    ptr->driver->close(ptr);
  }
}

void ImageDestroy() {}
void ImageInitialize() {}

int ImageOpen(const char* imagefilename, DiskImagePtr_t* hDiskImage_,
              bool* pWriteProtected_, bool bCreateIfNecessary) {
  DiskFormatDriver_t* driver = nullptr;
  void* instance = nullptr;
  DiskError_e err = DiskLoader_Open(imagefilename, bCreateIfNecessary,
                                    pWriteProtected_, &driver, &instance);
  if (err == DISK_ERR_NONE) {
    *hDiskImage_ = reinterpret_cast<DiskImagePtr_t>(instance);
    return IMAGE_ERROR_NONE;
  }
  return (err == DISK_ERR_FILE_NOT_FOUND) ? IMAGE_ERROR_UNABLE_TO_OPEN
                                          : IMAGE_ERROR_BAD_SIZE;
}

void ImageReadTrack(DiskImagePtr_t imageHandle, int track, int quartertrack,
                    uint8_t* trackImageBuffer, int* nibbles) {
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  if (track >= 0 && track < TRACKS && ptr->driver->read_track &&
      ptr->validTrack[track]) {
    ptr->driver->read_track(ptr, track, quartertrack, trackImageBuffer,
                            nibbles);
  } else {
    for (uint32_t i = 0; i < NIBBLES_PER_TRACK; ++i)
      trackImageBuffer[i] = static_cast<uint8_t>(rand() & 0xFF);
    *nibbles = static_cast<int>(NIBBLES_PER_TRACK);
  }
}

void ImageWriteTrack(DiskImagePtr_t imageHandle, int track, int quartertrack,
                     uint8_t* trackimage, int nibbles) {
  imageinfoptr ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  if (ptr->driver->write_track && !ptr->writeProtected) {
    ptr->driver->write_track(ptr, track, quartertrack, trackimage, nibbles);
  }
}
