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

#include "apple2/Disk.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "apple2/CPU.h"
#include "apple2/DiskCommands.h"
#include "apple2/DiskFTP.h"
#include "apple2/DiskGCR.h"
#include "apple2/DiskLoader.h"
#include "apple2/Memory.h"
#include "apple2/Structs.h"
#include "apple2/Video.h"
#include "apple2/formats/DoDriver.h"
#include "apple2/formats/IieDriver.h"
#include "apple2/formats/Nb2Driver.h"
#include "apple2/formats/NibDriver.h"
#include "apple2/formats/PoDriver.h"
#include "apple2/formats/Woz2Driver.h"
#include "apple2/ftpparse.h"
#include "core/Common_Globals.h"
#include "core/LinAppleCore.h"
#include "core/Log.h"
#include "core/Peripheral.h"
#include "core/Peripheral_Internal.h"
#include "core/Registry.h"
#include "core/Util_Path.h"
#include "core/Util_Text.h"

HostInterface_t* g_pDiskHost = nullptr;
int g_nDiskSlot = 0;

extern void Linapple_UpdateTitle(const char* title);
extern bool ImageBoot(DiskImagePtr_t);

static void CheckSpinning();
static auto GetDriveLightStatus(const int iDrive) -> Disk_Status_e;
static auto IsDriveValid(const int iDrive) -> bool;
static void ReadTrack(int drive);
static void RemoveDisk(int drive);
static void WriteTrack(int drive);

static DiskError_e DiskInsert_Internal(int drive, const char* imageFileName,
                                       bool writeProtected,
                                       bool createIfNecessary);

static PeripheralStatus Disk_ABI_Command(void* instance, uint32_t cmd,
                                         const void* data, size_t size);

static PeripheralStatus Disk_ABI_Query(void* instance, uint32_t cmd, void* data,
                                       size_t* size);

char Disk2_rom[] =
    "\xA2\x20\xA0\x00\xA2\x03\x86\x3C\x8A\x0A\x24\x3C\xF0\x10\x05\x3C"
    "\x49\xFF\x29\x7E\xB0\x08\x4A\xD0\xFB\x98\x9D\x56\x03\xC8\xE8\x10"
    "\xE5\x20\x58\xFF\xBA\xBD\x00\x01\x0A\x0A\x0A\x0A\x85\x2B\xAA\xBD"
    "\x8E\xC0\xBD\x8C\xC0\xBD\x8A\xC0\xBD\x89\xC0\xA0\x50\xBD\x80\xC0"
    "\x98\x29\x03\x0A\x05\x2B\xAA\xBD\x81\xC0\xA9\x56\x20\xA8\xFC\x88"
    "\x10\xEB\x85\x26\x85\x3D\x85\x41\xA9\x08\x85\x27\x18\x08\xBD\x8C"
    "\xC0\x10\xFB\x49\xD5\xD0\xF7\xBD\x8C\xC0\x10\xFB\xC9\xAA\xD0\xF3"
    "\xEA\xBD\x8C\xC0\x10\xFB\xC9\x96\xF0\x09\x28\x90\xDF\x49\xAD\xF0"
    "\x25\xD0\xD9\xA0\x03\x85\x40\xBD\x8C\xC0\x10\xFB\x2A\x85\x3C\xBD"
    "\x8C\xC0\x10\xFB\x25\x3C\x88\xD0\xEC\x28\xC5\x3D\xD0\xBE\xA5\x40"
    "\xC5\x41\xD0\xB8\xB0\xB7\xA0\x56\x84\x3C\xBC\x8C\xC0\x10\xFB\x59"
    "\xD6\x02\xA4\x3C\x88\x99\x00\x03\xD0\xEE\x84\x3C\xBC\x8C\xC0\x10"
    "\xFB\x59\xD6\x02\xA4\x3C\x91\x26\xC8\xD0\xEF\xBC\x8C\xC0\x10\xFB"
    "\x59\xD6\x02\xD0\x87\xA0\x00\xA2\x56\xCA\x30\xFB\xB1\x26\x5E\x00"
    "\x03\x2A\x5E\x00\x03\x2A\x91\x26\xC8\xD0\xEE\xE6\x27\xE6\x3D\xA5"
    "\x3D\xCD\x00\x08\xA6\x2B\x90\xDB\x4C\x01\x08\x00\x00\x00\x00\x00";

static auto DiskControlMotor(uint16_t pc, uint16_t addr, uint8_t bWrite,
                             uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

static auto DiskControlStepper(uint16_t pc, uint16_t addr, uint8_t bWrite,
                               uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

static auto DiskEnable(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d,
                       uint32_t nCyclesLeft) -> uint8_t;

static auto DiskReadWrite(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d,
                          uint32_t nCyclesLeft) -> uint8_t;

static auto DiskSetLatchValue(uint16_t pc, uint16_t addr, uint8_t bWrite,
                              uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

static auto DiskSetReadMode(uint16_t pc, uint16_t addr, uint8_t bWrite,
                            uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

static auto DiskSetWriteMode(uint16_t pc, uint16_t addr, uint8_t bWrite,
                             uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

#define LOG_DISK_ENABLED 0

#if (LOG_DISK_ENABLED)
#define LOG_DISK(format, ...) Logger::Info(format, __VA_ARGS__)
#else
#define LOG_DISK(...)
#endif

bool enhancedisk = true;

static uint16_t currdrive = 0;
static bool diskaccessed = false;
static Disk_t g_aFloppyDisk[DRIVES];
static uint8_t floppylatch = 0;
static bool floppymotoron = false;
static bool floppywritemode = false;
static uint16_t phases;  // state bits for stepper magnet phases 0 - 3

void CheckSpinning() {
  bool was_spinning = (g_aFloppyDisk[currdrive].spinning > 0);
  if (floppymotoron) {
    g_aFloppyDisk[currdrive].spinning = 20000;
  }
  bool now_spinning = (g_aFloppyDisk[currdrive].spinning > 0);

  if (was_spinning != now_spinning) {
    if (g_pDiskHost) {
      g_pDiskHost->NotifyActivityChanged(g_nDiskSlot, now_spinning);
      g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
    }
  }
}

auto DiskIsEffectivelyWriteProtected(const int iDrive) -> bool {
  if (iDrive < 0 || iDrive >= DRIVES) return false;
  Disk_t* fptr = &g_aFloppyDisk[iDrive];

  // User toggle (Layer 3) always wins if set to TRUE.
  if (fptr->user_write_protected) return true;

  // Otherwise, it's protected if either OS (Layer 2) or Format (Layer 1) says so.
  if (fptr->os_readonly) return true;

  if (fptr->driver) {
    if (!(fptr->driver->capabilities & DRIVER_CAP_WRITE)) return true;
    return fptr->driver->is_write_protected(fptr->driver_instance);
  }

  return false;
}

auto GetDriveLightStatus(const int iDrive) -> Disk_Status_e {
  if (IsDriveValid(iDrive)) {
    Disk_t* pFloppy = &g_aFloppyDisk[iDrive];

    if (pFloppy->spinning) {
      if (pFloppy->writelight) {
        return DISK_STATUS_WRITE;
      } else {
        return DISK_STATUS_READ;
      }
    } else {
      if (pFloppy->driver && DiskIsEffectivelyWriteProtected(iDrive)) {
        return DISK_STATUS_PROT;
      }
      return DISK_STATUS_OFF;
    }
  }

  return DISK_STATUS_OFF;
}

auto GetImageTitle(const char* imageFileName, Disk_t* fptr) -> char* {
  char imagetitle[MAX_DISK_FULL_NAME + 1];
  const char* startpos = imageFileName;

  const char* last_sep = strrchr(startpos, FILE_SEPARATOR);
  if (last_sep) {
    startpos = last_sep + 1;
  }
  Util_SafeStrCpy(imagetitle, startpos, MAX_DISK_FULL_NAME);

  bool found = false;
  int loop = 0;
  while (imagetitle[loop] && !found) {
    if (IsCharLower(imagetitle[loop])) {
      found = true;
    } else {
      loop++;
    }
  }

  if ((!found) && (loop > 2)) {
    for (char* p = imagetitle + 1; *p; ++p)
      *p = static_cast<char>(tolower(static_cast<uint8_t>(*p)));
  }

  Util_SafeStrCpy(fptr->fullname, imageFileName, MAX_DISK_FULL_NAME);

  if (imagetitle[0]) {
    char* dot = strrchr(imagetitle, '.');
    if (dot && dot > imagetitle) {
      *dot = 0;
    }
  }

  Util_SafeStrCpy(fptr->imagename, imagetitle, MAX_DISK_IMAGE_NAME);
  return fptr->imagename;
}

auto IsDriveValid(const int iDrive) -> bool {
  return (iDrive >= 0 && iDrive < DRIVES);
}

static void AllocTrack(int drive) {
  Disk_t* fptr = &g_aFloppyDisk[drive];
  const int NIBBLES_HARDWARE = 6656;
  fptr->trackimage = static_cast<uint8_t*>(malloc(NIBBLES_HARDWARE));
  if (fptr->trackimage) memset(fptr->trackimage, 0, NIBBLES_HARDWARE);
}

static void ReadTrack(int iDrive) {
  if (!IsDriveValid(iDrive)) {
    return;
  }

  Disk_t* pFloppy = &g_aFloppyDisk[iDrive];

  if (pFloppy->track >= TRACKS) {
    pFloppy->trackimagedata = false;
    return;
  }

  if (!pFloppy->trackimage) {
    AllocTrack(iDrive);
  }

  if (pFloppy->trackimage && pFloppy->driver && pFloppy->driver_instance) {
    LOG_DISK("read track %2X%s\r", pFloppy->track,
             (pFloppy->phase & 1) ? ".5" : "");

    pFloppy->driver->read_track(pFloppy->driver_instance, pFloppy->track,
                                pFloppy->phase, pFloppy->trackimage,
                                &pFloppy->nibbles);

    pFloppy->byte = 0;
    pFloppy->trackimagedata = (pFloppy->nibbles != 0);
  }
}

static void RemoveDisk(int iDrive) {
  Disk_t* pFloppy = &g_aFloppyDisk[iDrive];

  if (pFloppy->driver) {
    if (pFloppy->trackimage && pFloppy->trackimagedirty) {
      WriteTrack(iDrive);
    }

    pFloppy->driver->close(pFloppy->driver_instance);
    pFloppy->driver = nullptr;
    pFloppy->driver_instance = nullptr;

    if (pFloppy->trackimage) {
      free(pFloppy->trackimage);
      pFloppy->trackimage = nullptr;
    }

    pFloppy->trackimagedata = false;

    if (g_pDiskHost) {
      const char* key =
          (iDrive == 0) ? REGVALUE_DISK_IMAGE1 : REGVALUE_DISK_IMAGE2;
      g_pDiskHost->SetConfig("Slots", key, "");
      g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
    }
  }

  pFloppy->os_readonly = false;
  pFloppy->user_write_protected = false;
  pFloppy->last_error = DISK_ERR_NONE;
  memset(pFloppy->imagename, 0, MAX_DISK_IMAGE_NAME + 1);
  memset(pFloppy->fullname, 0, MAX_DISK_FULL_NAME + 1);
}

static void WriteTrack(int iDrive) {
  Disk_t* pFloppy = &g_aFloppyDisk[iDrive];

  if (pFloppy->track >= TRACKS) {
    return;
  }

  // Triple-layer write-protect check
  if (pFloppy->user_write_protected) return;
  if (pFloppy->os_readonly) return;
  if (!pFloppy->driver || !(pFloppy->driver->capabilities & DRIVER_CAP_WRITE))
    return;
  if (pFloppy->driver->is_write_protected(pFloppy->driver_instance)) return;

  if (pFloppy->trackimage && pFloppy->driver_instance) {
    pFloppy->driver->write_track(pFloppy->driver_instance, pFloppy->track,
                                 pFloppy->phase, pFloppy->trackimage,
                                 pFloppy->nibbles);
  }

  pFloppy->trackimagedirty = false;
}

// All globally accessible functions are below this line

void DiskBoot() {
  if (g_aFloppyDisk[0].driver && ImageBoot(static_cast<DiskImagePtr_t>(
                                     g_aFloppyDisk[0].driver_instance))) {
    floppymotoron = false;
  }
}

static auto DiskControlMotor(uint16_t, uint16_t address, uint8_t, uint8_t,
                             uint32_t) -> uint8_t {
  floppymotoron = (address & 1) != 0;
  CheckSpinning();
  return MemReturnRandomData(1);
}

static auto DiskControlStepper(uint16_t, uint16_t address, uint8_t, uint8_t,
                               uint32_t) -> uint8_t {
  Disk_t* fptr = &g_aFloppyDisk[currdrive];
  int phase = (address >> 1) & 3;
  int phase_bit = (1 << phase);

  if (address & 1) {
    phases |= phase_bit;
  } else {
    phases &= ~phase_bit;
  }

  int direction = 0;
  if (phases & (1 << ((fptr->phase + 1) & 3))) {
    direction += 1;
  }
  if (phases & (1 << ((fptr->phase + 3) & 3))) {
    direction -= 1;
  }

  if (direction) {
    fptr->phase = MAX(0, MIN(79, fptr->phase + direction));
    int newtrack = MIN(TRACKS - 1, fptr->phase >> 1);
    if (newtrack != fptr->track) {
      if (fptr->trackimage && fptr->trackimagedirty) {
        WriteTrack(currdrive);
      }
      fptr->track = newtrack;
      fptr->trackimagedata = false;
    }
  }
  return (address == 0xE0) ? 0xFF : MemReturnRandomData(1);
}

void DiskDestroy() {
  DiskLoader_Shutdown();
  RemoveDisk(0);
  RemoveDisk(1);
}

static auto DiskEnable(uint16_t, uint16_t address, uint8_t, uint8_t, uint32_t)
    -> uint8_t {
  currdrive = address & 1;
  g_aFloppyDisk[!currdrive].spinning = 0;
  g_aFloppyDisk[!currdrive].writelight = 0;
  CheckSpinning();
  return 0;
}

void DiskEject(const int iDrive) {
  if (IsDriveValid(iDrive)) {
    RemoveDisk(iDrive);
    if (g_pDiskHost) {
      const char* key =
          (iDrive == 0) ? REGVALUE_DISK_IMAGE1 : REGVALUE_DISK_IMAGE2;
      g_pDiskHost->SetConfig("Slots", key, "");
    }
  }
}

auto DiskGetFullName(int drive) -> const char* {
  return g_aFloppyDisk[drive].fullname;
}

void DiskGetLightStatus(int* pDisk1Status_, int* pDisk2Status_) {
  if (pDisk1Status_) {
    *pDisk1Status_ = GetDriveLightStatus(0);
  }
  if (pDisk2Status_) {
    *pDisk2Status_ = GetDriveLightStatus(1);
  }
}

auto DiskGetName(int drive) -> const char* {
  return g_aFloppyDisk[drive].imagename;
}

void DiskInitialize() {
  int loop = DRIVES;
  while (loop--) {
    memset(&g_aFloppyDisk[loop], 0, sizeof(Disk_t));
  }
}

static auto DiskInsert_Internal(int drive, const char* imageFileName,
                                bool writeProtected,
                                bool createIfNecessary) -> DiskError_e {
  Disk_t* fptr = &g_aFloppyDisk[drive];

  if (fptr->driver) {
    RemoveDisk(drive);
  }
  memset(fptr, 0, sizeof(Disk_t));

  fptr->user_write_protected = writeProtected;
  fptr->os_readonly = false;
  DiskError_e error = DiskLoader_Open(
      imageFileName, createIfNecessary, &fptr->os_readonly,
      const_cast<DiskFormatDriver_t**>(&fptr->driver), &fptr->driver_instance);
  fptr->last_error = error;
  if (error == DISK_ERR_NONE) {
    char* tmp = GetImageTitle(imageFileName, fptr);
    char s_title[MAX_DISK_IMAGE_NAME + 32];
    snprintf(s_title, sizeof(s_title), "%.*s - %.*s",
             static_cast<int>(strlen(g_pAppTitle)), g_pAppTitle,
             static_cast<int>(strlen(tmp)), tmp);
    if (drive == 0) {
      Linapple_UpdateTitle(s_title);
    }

    if (g_pDiskHost) {
      const char* key =
          (drive == 0) ? REGVALUE_DISK_IMAGE1 : REGVALUE_DISK_IMAGE2;
      g_pDiskHost->SetConfig("Slots", key, imageFileName);
      g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
    }
  } else {
    if (g_pDiskHost) {
      g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
    }
  }
  return error;
}

auto DiskInsert(int drive, const char* imageFileName, bool writeProtected,
                bool createIfNecessary) -> int {
  return static_cast<int>(DiskInsert_Internal(
      drive, imageFileName, writeProtected, createIfNecessary));
}

auto DiskIsSpinning() -> bool { return floppymotoron; }

auto DiskGetProtect(const int iDrive) -> bool {
  if (IsDriveValid(iDrive)) {
    return g_aFloppyDisk[iDrive].user_write_protected;
  }
  return false;
}

void DiskSetProtect(const int iDrive, const bool bWriteProtect) {
  if (IsDriveValid(iDrive)) {
    g_aFloppyDisk[iDrive].user_write_protected = bWriteProtect;
    if (g_pDiskHost) {
      g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
    }
  }
}
static auto DiskReadWrite(uint16_t programcounter, uint16_t, uint8_t, uint8_t,
                          uint32_t) -> uint8_t {
  (void)programcounter;
  Disk_t* fptr = &g_aFloppyDisk[currdrive];
  diskaccessed = true;
  if ((!fptr->trackimagedata) && fptr->driver) {
    ReadTrack(currdrive);
  }
  if (!fptr->trackimagedata) {
    return 0xFF;
  }
  uint8_t result = 0;

  bool is_protected = fptr->user_write_protected || fptr->os_readonly;
  if (fptr->driver) {
    if (!(fptr->driver->capabilities & DRIVER_CAP_WRITE))
      is_protected = true;
    else if (fptr->driver->is_write_protected(fptr->driver_instance))
      is_protected = true;
  }

  if ((!floppywritemode) || (!is_protected)) {
    if (floppywritemode) {
      if (floppylatch & 0x80) {
        *(fptr->trackimage + fptr->byte) = floppylatch;
        fptr->trackimagedirty = true;
      } else {
        return 0;
      }
    } else {
      result = *(fptr->trackimage + fptr->byte);
    }
  }
  if (++fptr->byte >= fptr->nibbles) {
    fptr->byte = 0;
  }
  return result;
}

void DiskReset() {
  floppymotoron = false;
  phases = 0;
}

static auto DiskSetLatchValue(uint16_t, uint16_t, uint8_t write, uint8_t value,
                              uint32_t) -> uint8_t {
  if (write) {
    floppylatch = value;
  }
  return floppylatch;
}

static auto DiskSetReadMode(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t)
    -> uint8_t {
  floppywritemode = false;
  return MemReturnRandomData(DiskIsEffectivelyWriteProtected(currdrive));
}

static auto DiskSetWriteMode(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t)
    -> uint8_t {
  floppywritemode = true;
  bool modechange = !g_aFloppyDisk[currdrive].writelight;
  g_aFloppyDisk[currdrive].writelight = 20000;
  if (modechange) {
    if (g_pDiskHost) g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
  }
  return MemReturnRandomData(1);
}

void DiskUpdatePosition(uint32_t cycles) {
  int loop = 2;
  while (loop--) {
    Disk_t* fptr = &g_aFloppyDisk[loop];
    if (fptr->spinning && !floppymotoron) {
      if (!(fptr->spinning -= MIN(fptr->spinning, (cycles >> 6)))) {
        if (g_pDiskHost) {
          g_pDiskHost->NotifyActivityChanged(g_nDiskSlot, false);
          g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
        }
      }
    }
    if (floppywritemode && (currdrive == loop) && fptr->spinning) {
      fptr->writelight = 20000;
    } else if (fptr->writelight) {
      if (!(fptr->writelight -= MIN(fptr->writelight, (cycles >> 6)))) {
        if (g_pDiskHost) g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
      }
    }
    if ((!enhancedisk) && (!diskaccessed) && fptr->spinning) {
      if (g_pDiskHost) g_pDiskHost->RequestPreciseTiming();
      fptr->byte += (cycles >> 5);
      if (fptr->byte >= fptr->nibbles) {
        fptr->byte -= fptr->nibbles;
      }
    }
  }
  diskaccessed = false;
}

auto DiskDriveSwap() -> bool {
  if (g_aFloppyDisk[0].spinning || g_aFloppyDisk[1].spinning) {
    return false;
  }

  Disk_t temp{};
  memcpy(&temp, &g_aFloppyDisk[0], sizeof(Disk_t));
  memcpy(&g_aFloppyDisk[0], &g_aFloppyDisk[1], sizeof(Disk_t));
  memcpy(&g_aFloppyDisk[1], &temp, sizeof(Disk_t));

  char s_title[MAX_DISK_IMAGE_NAME + 32];
  snprintf(s_title, MAX_DISK_IMAGE_NAME + 32, "%.*s - %.*s",
           static_cast<int>(strlen(g_pAppTitle)), g_pAppTitle,
           static_cast<int>(strlen(g_aFloppyDisk[0].imagename)),
           g_aFloppyDisk[0].imagename);
  Linapple_UpdateTitle(s_title);

  if (g_pDiskHost) {
    g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);
  }

  return true;
}

auto Disk_IORead(void* instance, uint16_t pc, uint16_t addr, uint8_t bWrite,
                 uint8_t d, uint32_t nCyclesLeft) -> uint8_t;
auto Disk_IOWrite(void* instance, uint16_t pc, uint16_t addr, uint8_t bWrite,
                  uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

static auto Disk_ABI_Init(int slot, HostInterface_t* host) -> void* {
  g_pDiskHost = host;
  g_nDiskSlot = slot;

  DiskLoader_Init();
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_woz2_driver));
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_iie_driver));
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_nib_driver));
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_nb2_driver));
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_do_driver));
  DiskLoader_Register(const_cast<DiskFormatDriver_t*>(&g_po_driver));

  DiskInitialize();

  const char* img1 = host->GetConfig("Slots", REGVALUE_DISK_IMAGE1);
  char path1[PATH_MAX_LEN] = {0};
  if (img1) {
    Util_SafeStrCpy(path1, img1, sizeof(path1));
  }

  const char* img2 = host->GetConfig("Slots", REGVALUE_DISK_IMAGE2);
  char path2[PATH_MAX_LEN] = {0};
  if (img2) {
    Util_SafeStrCpy(path2, img2, sizeof(path2));
  }

  if (path1[0]) {
    DiskInsert_Internal(0, path1, false, false);
  }
  if (path2[0]) {
    DiskInsert_Internal(1, path2, false, false);
  }

  static uint8_t patched_rom[256];
  memcpy(patched_rom, Disk2_rom, 256);
  // TODO/FIXME: HACK! REMOVE A WAIT ROUTINE FROM THE DISK CONTROLLER'S FIRMWARE
  patched_rom[0x4C] = 0xA9;
  patched_rom[0x4D] = 0x00;
  patched_rom[0x4E] = 0xEA;
  host->RegisterCxROM(slot, patched_rom);

  host->RegisterIO(slot, Disk_IORead, Disk_IOWrite, nullptr, nullptr);

  return reinterpret_cast<void*>(1);  // Dummy instance
}

static void Disk_ABI_Reset(void* instance) {
  (void)instance;
  DiskReset();
}

static void Disk_ABI_Shutdown(void* instance) {
  (void)instance;
  DiskDestroy();
}

static auto Disk_ABI_Think(void* instance, uint32_t cycles) -> void {
  (void)instance;
  DiskUpdatePosition(cycles);
}

static void PopulateDiskStatus(DiskStatus_t* status) {
  Disk_t* d0 = &g_aFloppyDisk[0];
  Disk_t* d1 = &g_aFloppyDisk[1];

  status->drive0_loaded = (d0->driver != nullptr);
  status->drive0_spinning = (d0->spinning > 0);
  status->drive0_writing = (d0->writelight > 0);
  status->drive0_write_protected = DiskIsEffectivelyWriteProtected(0);
  status->drive0_last_error = d0->last_error;
  Util_SafeStrCpy(status->drive0_name, d0->imagename, DISK_STATUS_NAME_MAX);
  Util_SafeStrCpy(status->drive0_full_path, d0->fullname, DISK_STATUS_PATH_MAX);

  status->drive1_loaded = (d1->driver != nullptr);
  status->drive1_spinning = (d1->spinning > 0);
  status->drive1_writing = (d1->writelight > 0);
  status->drive1_write_protected = DiskIsEffectivelyWriteProtected(1);
  status->drive1_last_error = d1->last_error;
  Util_SafeStrCpy(status->drive1_name, d1->imagename, DISK_STATUS_NAME_MAX);
  Util_SafeStrCpy(status->drive1_full_path, d1->fullname, DISK_STATUS_PATH_MAX);
}

static auto Disk_ABI_Command(void* instance, uint32_t cmd, const void* data,
                             size_t size) -> PeripheralStatus {
  (void)instance;
  switch (cmd) {
    case DISK_CMD_INSERT: {
      if (!data || size < sizeof(DiskInsertCmd_t)) return PERIPHERAL_ERROR;
      auto* c = static_cast<const DiskInsertCmd_t*>(data);
      if (!IsDriveValid(c->drive)) return PERIPHERAL_ERROR;
      DiskInsert_Internal(c->drive, c->path, c->write_protected,
                          c->create_if_necessary);
      return PERIPHERAL_OK;
    }
    case DISK_CMD_EJECT: {
      if (!data || size < sizeof(DiskEjectCmd_t)) return PERIPHERAL_ERROR;
      auto* c = static_cast<const DiskEjectCmd_t*>(data);
      if (!IsDriveValid(c->drive)) return PERIPHERAL_ERROR;
      DiskEject(c->drive);
      return PERIPHERAL_OK;
    }
    case DISK_CMD_SWAP_DRIVES:
      DiskDriveSwap();
      return PERIPHERAL_OK;
    case DISK_CMD_SET_PROTECT: {
      if (!data || size < sizeof(DiskSetProtectCmd_t)) return PERIPHERAL_ERROR;
      auto* c = static_cast<const DiskSetProtectCmd_t*>(data);
      if (!IsDriveValid(c->drive)) return PERIPHERAL_ERROR;
      DiskSetProtect(c->drive, c->write_protected);
      return PERIPHERAL_OK;
    }
    default:
      return PERIPHERAL_ERROR;
  }
}

static auto Disk_ABI_Query(void* instance, uint32_t cmd, void* data,
                           size_t* size) -> PeripheralStatus {
  (void)instance;
  if (cmd == DISK_CMD_GET_STATUS) {
    if (!data || !size || *size < sizeof(DiskStatus_t)) {
      if (size) *size = sizeof(DiskStatus_t);
      return PERIPHERAL_ERROR;
    }
    PopulateDiskStatus(static_cast<DiskStatus_t*>(data));
    *size = sizeof(DiskStatus_t);
    return PERIPHERAL_OK;
  }
  return PERIPHERAL_ERROR;
}

Peripheral_t g_disk_peripheral = {LINAPPLE_ABI_VERSION,
                                  "Disk II",
                                  0xFE,  // Slots 1-7
                                  Disk_ABI_Init,
                                  Disk_ABI_Reset,
                                  Disk_ABI_Shutdown,
                                  Disk_ABI_Think,
                                  nullptr,  // on_vblank
                                  nullptr,  // save_state (TODO: #247)
                                  nullptr,  // load_state (TODO: #247)
                                  Disk_ABI_Command,
                                  Disk_ABI_Query};

#ifdef BUILD_SHARED_PERIPHERAL
EXPORT_PERIPHERAL(g_disk_peripheral)
#endif

auto Disk_IORead(void* instance, uint16_t pc, uint16_t addr, uint8_t bWrite,
                 uint8_t d, uint32_t nCyclesLeft) -> uint8_t {
  (void)instance;
  addr &= 0xFF;

  switch (addr & 0xf) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
      return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
    case 0x8:
    case 0x9:
      return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
    case 0xA:
    case 0xB:
      return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
    case 0xC:
      return DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft);
    case 0xD:
      return DiskSetLatchValue(pc, addr, bWrite, d, nCyclesLeft);
    case 0xE:
      return DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft);
    case 0xF:
      return DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft);
  }

  return 0;
}

auto Disk_IOWrite(void* instance, uint16_t pc, uint16_t addr, uint8_t bWrite,
                  uint8_t d, uint32_t nCyclesLeft) -> uint8_t {
  (void)instance;
  addr &= 0xFF;

  switch (addr & 0xf) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
      return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
    case 0x8:
    case 0x9:
      return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
    case 0xA:
    case 0xB:
      return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
    case 0xC:
      return DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft);
    case 0xD:
      return DiskSetLatchValue(pc, addr, bWrite, d, nCyclesLeft);
    case 0xE:
      return DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft);
    case 0xF:
      return DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft);
  }

  return 0;
}

auto DiskGetSnapshot(SS_CARD_DISK2* pSS, uint32_t dwSlot) -> uint32_t {
  pSS->Hdr.UnitHdr.dwLength = sizeof(SS_CARD_DISK2);
  pSS->Hdr.UnitHdr.dwVersion = MAKE_VERSION(1, 0, 0, 2);

  pSS->Hdr.dwSlot = dwSlot;
  pSS->Hdr.dwType = CT_Disk2;

  pSS->phases = phases;
  pSS->currdrive = currdrive;
  pSS->diskaccessed = diskaccessed;
  pSS->enhancedisk = enhancedisk;
  pSS->floppylatch = floppylatch;
  pSS->floppymotoron = floppymotoron;
  pSS->floppywritemode = floppywritemode;

  for (uint32_t i = 0; i < 2; i++) {
    strcpy(pSS->Unit[i].szFileName, g_aFloppyDisk[i].fullname);
    pSS->Unit[i].track = g_aFloppyDisk[i].track;
    pSS->Unit[i].phase = g_aFloppyDisk[i].phase;
    pSS->Unit[i].byte = g_aFloppyDisk[i].byte;
    pSS->Unit[i].writeprotected = DiskGetProtect(i);
    pSS->Unit[i].trackimagedata = g_aFloppyDisk[i].trackimagedata;
    pSS->Unit[i].trackimagedirty = g_aFloppyDisk[i].trackimagedirty;
    pSS->Unit[i].spinning = g_aFloppyDisk[i].spinning;
    pSS->Unit[i].writelight = g_aFloppyDisk[i].writelight;
    pSS->Unit[i].nibbles = g_aFloppyDisk[i].nibbles;

    if (g_aFloppyDisk[i].trackimage) {
      memcpy(pSS->Unit[i].nTrack, g_aFloppyDisk[i].trackimage,
             NIBBLES_PER_TRACK);
    } else {
      memset(pSS->Unit[i].nTrack, 0, NIBBLES_PER_TRACK);
    }
  }

  return 0;
}

auto DiskSetSnapshot(SS_CARD_DISK2* pSS, uint32_t) -> uint32_t {
  if (pSS->Hdr.UnitHdr.dwVersion > MAKE_VERSION(1, 0, 0, 2)) {
    return static_cast<uint32_t>(-1);
  }
  phases = pSS->phases;
  currdrive = pSS->currdrive;
  diskaccessed = pSS->diskaccessed;
  enhancedisk = pSS->enhancedisk;
  floppylatch = pSS->floppylatch;
  floppymotoron = pSS->floppymotoron;
  floppywritemode = pSS->floppywritemode;

  for (uint32_t i = 0; i < 2; i++) {
    bool bImageError = false;

    memset(&g_aFloppyDisk[i], 0, sizeof(Disk_t));
    if (pSS->Unit[i].szFileName[0] == 0x00) {
      continue;
    }

    if (DiskInsert(i, pSS->Unit[i].szFileName, false, false)) {
      bImageError = true;
    }
    g_aFloppyDisk[i].track = pSS->Unit[i].track;
    g_aFloppyDisk[i].phase = pSS->Unit[i].phase;
    g_aFloppyDisk[i].byte = pSS->Unit[i].byte;
    g_aFloppyDisk[i].trackimagedata = pSS->Unit[i].trackimagedata;
    g_aFloppyDisk[i].trackimagedirty = pSS->Unit[i].trackimagedirty;
    g_aFloppyDisk[i].spinning = pSS->Unit[i].spinning;
    g_aFloppyDisk[i].writelight = pSS->Unit[i].writelight;
    g_aFloppyDisk[i].nibbles = pSS->Unit[i].nibbles;

    if (!bImageError) {
      if ((g_aFloppyDisk[i].trackimage == nullptr) &&
          g_aFloppyDisk[i].nibbles) {
        AllocTrack(i);
      }

      if (g_aFloppyDisk[i].trackimage == nullptr) {
        bImageError = true;
      } else {
        memcpy(g_aFloppyDisk[i].trackimage, pSS->Unit[i].nTrack,
               NIBBLES_PER_TRACK);
      }
    }

    if (bImageError) {
      g_aFloppyDisk[i].trackimagedata = false;
      g_aFloppyDisk[i].trackimagedirty = false;
      g_aFloppyDisk[i].nibbles = 0;
    }
  }

  if (g_pDiskHost) g_pDiskHost->NotifyStatusChanged(g_nDiskSlot);

  return 0;
}
