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

/* Description: Disk Image
 *
 * Author: Various
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include "core/Common.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "apple2/Disk.h"
#include "apple2/DiskImage.h"
#include "apple2/Structs.h"
#include "core/Util_Path.h"
#include "core/Util_Text.h"
#include "core/Log.h"
#include "core/Common_Globals.h"
#include "apple2/Memory.h"
#include "apple2/CPU.h"

const int BITS_PER_BYTE = 8;

/* DO logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F */
/*    physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F */

/* PO logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F */
/*    physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F */

using imageinforec = struct imageinforec {
  char filename[MAX_PATH];
  uint32_t format;
  FilePtr file;
  uint32_t offset;
  bool writeProtected;
  uint32_t headerSize;
  std::unique_ptr<uint8_t[], void(*)(void*)> header;
  bool validTrack[TRACKS];

  imageinforec() : file(nullptr, fclose), header(nullptr, free) {}
};
using imageinfoptr = imageinforec*;

using boottype = bool (*  )(imageinfoptr);

using detecttype = uint32_t(*)(uint8_t*, uint32_t);

using readtype = void (*  )(imageinfoptr, int, int, uint8_t*, int *);

using writetype = void (* )(imageinfoptr, int, int, uint8_t*, int);

static auto Util_GetFileSize(FILE* f) -> size_t {
  long current = ftell(f);
  fseek(f, 0, SEEK_END);
  size_t size = static_cast<size_t>(ftell(f));
  fseek(f, current, SEEK_SET);
  return size;
}

auto AplBoot(imageinfoptr ptr) -> bool;

auto AplDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

auto DoDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void DoRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);

void DoWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles);

auto IieDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void IieRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);

void IieWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles);

auto Nib1Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void Nib1Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);

void Nib1Write(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles);

auto Nib2Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void Nib2Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);

void Nib2Write(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles);

auto PoDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void PoRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);

void PoWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles);

auto PrgBoot(imageinfoptr ptr) -> bool;

auto PrgDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

auto Woz2Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t;

void Woz2Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles);


using imagetyperec = struct imagetyperec {
  const char* createExts;
  const char* rejectExts;
  detecttype detect;
  boottype boot;
  readtype read;
  writetype write;
};
using imagetypeptr = imagetyperec*;

enum ImageTypeIndex_e
{
  IMG_TYPE_PRG = 0,
  IMG_TYPE_DO  = 1,
  IMG_TYPE_PO  = 2,
  IMG_TYPE_APL = 3,
  IMG_TYPE_NIB = 4,
  IMG_TYPE_NB2 = 5,
  IMG_TYPE_IIE = 6,
  IMG_TYPE_WOZ = 7,
};

static imagetyperec imagetype[IMAGETYPES] = {{".prg",
  ".do;.dsk;.iie;.nib;.po",                                                                      PrgDetect,  PrgBoot, nullptr,     nullptr},
                                             {".do;.dsk",
                                               ".nib;.iie;.po;.prg",                           DoDetect,   nullptr,    DoRead,   DoWrite},
                                             {".po",
                                               ".do;.iie;.nib;.prg",                           PoDetect,   nullptr,    PoRead,   PoWrite},
                                             {".apl",
                                               ".do;.dsk;.iie;.nib;.po",                       AplDetect,  AplBoot, nullptr,     nullptr},
                                             {".nib",
                                               ".do;.iie;.po;.prg",                            Nib1Detect, nullptr,    Nib1Read, Nib1Write},
                                             {".nb2",
                                               ".do;.iie;.po;.prg",                            Nib2Detect, nullptr,    Nib2Read, Nib2Write},
                                             {".iie",
                                               ".do.;.nib;.po;.prg",                           IieDetect,  nullptr,    IieRead,  IieWrite},
                                             {".woz",
                                     ".do;.dsk;.iie;.nib;.po;.prg",                            Woz2Detect, nullptr,    Woz2Read, nullptr}};

static uint8_t diskbyte[GCR_ENCODE_TABLE_SIZE] = {0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2,
                              0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE,
                              0xCF, 0xD3, 0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9,
                              0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB,
                              0xFC, 0xFD, 0xFE, 0xFF};

static uint8_t sectornumber[NUM_INTERLEAVE_MODES][SECTORS_PER_TRACK_16] = {{0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B, 0x04, 0x0C, 0x05, 0x0D, 0x06, 0x0E, 0x07, 0x0F},
                                     {0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04, 0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F},
                                     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static std::vector<uint8_t> workbuffer;

// Nibblization functions
auto Code62(int sector) -> uint8_t*
{

  // Convert the 256 8-bit bytes into 342 6-bit bytes, which we store
  // Starting at 4k into the work buffer.
  {
    uint8_t* sectorBase = &workbuffer[sector * PAGE_SIZE];
    uint8_t* resultptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    int resIdx = 0;
    uint8_t offset = 0xAC;
    while (offset != 0x02) {
      uint8_t value = 0;
      #define ADDVALUE(a) value = (value << 2) |        \
                            (((a) & 0x01) << 1) | \
                            (((a) & 0x02) >> 1)
      ADDVALUE(sectorBase[offset]);
      offset -= 0x56;
      ADDVALUE(sectorBase[offset]);
      offset -= 0x56;
      ADDVALUE(sectorBase[offset]);
      offset -= 0x53;
      #undef ADDVALUE
      resultptr[resIdx++] = value << 2;
    }
    if (resIdx >= 2) {
      resultptr[resIdx - 2] &= 0x3F;
      resultptr[resIdx - 1] &= 0x3F;
    }
    for (int loop = 0; loop < PAGE_SIZE; ++loop) {
      resultptr[resIdx++] = sectorBase[loop];
    }
  }

  // Exclusive-or the entire data block with itself offset by one byte,
  // Creating a 343rd byte which is used as a checksum. Store the new
  // Block of 343 bytes starting at 5k into the work buffer.
  {
    uint8_t savedval = 0;
    uint8_t* sourceptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    uint8_t* resultptr = &workbuffer[GCR_CHECKSUM_BUFFER_OFFSET];
    for (int loop = 0; loop < GCR_SECTOR_DATA_SIZE; ++loop) {
      resultptr[loop] = savedval ^ sourceptr[loop];
      savedval = sourceptr[loop];
    }
    resultptr[GCR_SECTOR_DATA_SIZE] = savedval;
  }

  // Using a lookup table, convert the 6-bit bytes into disk bytes. A valid
  // disk byte is a byte that has the high bit set, at least two adjacent
  // bits set (excluding the high bit), and at most one pair of consecutive
  // zero bits. The converted block of 343 bytes is stored starting at 4k
  // into the work buffer.
  {
    uint8_t* sourceptr = &workbuffer[GCR_CHECKSUM_BUFFER_OFFSET];
    uint8_t* resultptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    for (int loop = 0; loop < GCR_SECTOR_WITH_CHECKSUM_SIZE; ++loop) {
      resultptr[loop] = diskbyte[sourceptr[loop] >> 2];
    }
  }

  return &workbuffer[GCR_WORK_BUFFER_OFFSET];
}

void Decode62(uint8_t* imageptr)
{

  // If we haven't already done so, generate a table for converting
  // disk bytes back into 6-bit bytes
  static bool tablegenerated = false;
  static uint8_t sixbitbyte[GCR_DECODE_TABLE_SIZE];
  if (!tablegenerated) {
    memset(sixbitbyte, 0, GCR_DECODE_TABLE_SIZE);
    int loop = 0;
    while (loop < GCR_ENCODE_TABLE_SIZE) {
      sixbitbyte[diskbyte[loop] - 0x80] = loop << 2;
      loop++;
    }
    tablegenerated = true;
  }

  // Using our table, convert the disk bytes back into 6-bit bytes
  {
    uint8_t* sourceptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    uint8_t* resultptr = &workbuffer[GCR_CHECKSUM_BUFFER_OFFSET];
    for (int loop = 0; loop < GCR_SECTOR_WITH_CHECKSUM_SIZE; ++loop) {
      resultptr[loop] = sixbitbyte[sourceptr[loop] & 0x7F];
    }
  }

  // Exclusive-or the entire data block with itself offset by one byte
  // to undo the effects of the checksumming process
  {
    uint8_t savedval = 0;
    uint8_t* sourceptr = &workbuffer[GCR_CHECKSUM_BUFFER_OFFSET];
    uint8_t* resultptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    for (int loop = 0; loop < GCR_SECTOR_DATA_SIZE; ++loop) {
      resultptr[loop] = savedval ^ sourceptr[loop];
      savedval = resultptr[loop];
    }
  }

  // Convert the 342 6-bit bytes into 256 8-bit bytes
  {
    uint8_t* lowbitsptr = &workbuffer[GCR_WORK_BUFFER_OFFSET];
    uint8_t* sectorBase = &workbuffer[GCR_WORK_BUFFER_OFFSET + 0x56];
    uint8_t offset = 0xAC;
    while (offset != 0x02) {
      if (offset >= 0xAC) {
        imageptr[offset] =
          (sectorBase[offset] & 0xFC) | ((lowbitsptr[0] & 0x80) >> 7) | ((lowbitsptr[0] & 0x40) >> 5);
      }
      offset -= 0x56;
      imageptr[offset] =
        (sectorBase[offset] & 0xFC) | ((lowbitsptr[0] & 0x20) >> 5) | ((lowbitsptr[0] & 0x10) >> 3);
      offset -= 0x56;
      imageptr[offset] =
        (sectorBase[offset] & 0xFC) | ((lowbitsptr[0] & 0x08) >> 3) | ((lowbitsptr[0] & 0x04) >> 1);
      offset -= 0x53;
      lowbitsptr++;
    }
  }
}

void DenibblizeTrack(uint8_t* trackimage, bool dosorder, int nibbles)
{
  memset(workbuffer.data(), 0, GCR_WORK_BUFFER_OFFSET);

  // Search through the track image for each sector. For every sector
  // we find, copy the nibblized data for that sector into the work
  // buffer at offset 4k. Then call decode62() to denibblize the data
  // in the buffer and write it into the first part of the work buffer
  // offset by the sector number.
  {
    int offset = 0;
    int partsleft = 33;
    int sector = 0;
    while (partsleft--) {
      uint8_t byteval[3] = {0, 0, 0};
      int bytenum = 0;
      int loop = nibbles;
      while ((loop--) && (bytenum < 3)) {
        if (bytenum) {
          byteval[bytenum++] = trackimage[offset++];
        } else if (trackimage[offset++] == 0xD5) {
          bytenum = 1;
        }
        if (offset >= nibbles) {
          offset = 0;
        }
      }
      if ((bytenum == 3) && (byteval[1] == 0xAA)) {
        int tempoffset = offset;
        const int SCAN_BUFFER_SIZE = 384;
        for (int i = 0; i < SCAN_BUFFER_SIZE; ++i) {
          workbuffer[GCR_WORK_BUFFER_OFFSET + i] = trackimage[tempoffset++];
          if (tempoffset >= nibbles) {
            tempoffset = 0;
          }
        }
        if (byteval[2] == 0x96) {
          sector = ((workbuffer[GCR_WORK_BUFFER_OFFSET + 4] & 0x55) << 1) | (workbuffer[GCR_WORK_BUFFER_OFFSET + 5] & 0x55);
        } else if (byteval[2] == 0xAD) {
          Decode62(&workbuffer[sectornumber[dosorder][sector] * PAGE_SIZE]);
          sector = 0;
        }
      }
    }
  }
}

auto NibblizeTrack(uint8_t* trackImageBuffer, bool dosorder, int track) -> uint32_t
{
  memset(&workbuffer[DOS_TRACK_SIZE], 0, DOS_TRACK_SIZE);
  uint32_t offset = 0;
  uint8_t sector = 0;

  // Write gap one, which contains 48 self-sync bytes
  const int GAP1_SIZE = 48;
  for (int loop = 0; loop < GAP1_SIZE; loop++) {
    trackImageBuffer[offset++] = 0xFF;
  }

  while (sector < SECTORS_PER_TRACK_16) {

    // Write the address field, which contains:
    //   - PROLOGUE (D5AA96)
    //   - VOLUME NUMBER ("4 AND 4" ENCODED)
    //   - TRACK NUMBER ("4 AND 4" ENCODED)
    //   - SECTOR NUMBER ("4 AND 4" ENCODED)
    //   - CHECKSUM ("4 AND 4" ENCODED)
    //   - EPILOGUE (DEAAEB)
    trackImageBuffer[offset++] = 0xD5;
    trackImageBuffer[offset++] = 0xAA;
    trackImageBuffer[offset++] = 0x96;
    #define VOLUME 0xFE
    #define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
    #define CODE44B(a) (((a) & 0x55) | 0xAA)
    trackImageBuffer[offset++] = CODE44A(VOLUME);
    trackImageBuffer[offset++] = CODE44B(VOLUME);
    trackImageBuffer[offset++] = CODE44A((uint8_t) track);
    trackImageBuffer[offset++] = CODE44B((uint8_t) track);
    trackImageBuffer[offset++] = CODE44A(sector);
    trackImageBuffer[offset++] = CODE44B(sector);
    trackImageBuffer[offset++] = CODE44A(VOLUME ^ ((uint8_t) track) ^ sector);
    trackImageBuffer[offset++] = CODE44B(VOLUME ^ ((uint8_t) track) ^ sector);
    #undef CODE44A
    #undef CODE44B
    trackImageBuffer[offset++] = 0xDE;
    trackImageBuffer[offset++] = 0xAA;
    trackImageBuffer[offset++] = 0xEB;

    // Write gap two, which contains six self-sync bytes
    const int GAP2_SIZE = 6;
    for (int loop = 0; loop < GAP2_SIZE; loop++) {
      trackImageBuffer[offset++] = 0xFF;
    }

    // Write the data field, which contains:
    //   - PROLOGUE (D5AAAD)
    //   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
    //   - EPILOGUE (DEAAEB)
    trackImageBuffer[offset++] = 0xD5;
    trackImageBuffer[offset++] = 0xAA;
    trackImageBuffer[offset++] = 0xAD;
    memcpy(trackImageBuffer + offset,  Code62(sectornumber[dosorder][sector]),  GCR_SECTOR_WITH_CHECKSUM_SIZE);
    offset += GCR_SECTOR_WITH_CHECKSUM_SIZE;
    trackImageBuffer[offset++] = 0xDE;
    trackImageBuffer[offset++] = 0xAA;
    trackImageBuffer[offset++] = 0xEB;

    // Write gap three, which contains 27 self-sync bytes
    for (int loop = 0; loop < GAP3_SIZE; loop++) {
      trackImageBuffer[offset++] = 0xFF;
    }

    sector++;
  }

  return offset;
}

void SkewTrack(int track, int nibbles, uint8_t* trackImageBuffer)
{
  const int SKEW_FACTOR = 768;
  int skewbytes = (track * SKEW_FACTOR) % nibbles;
  memcpy(workbuffer.data(),  trackImageBuffer,  static_cast<size_t>(nibbles));
  memcpy(trackImageBuffer,  &workbuffer[skewbytes],  static_cast<size_t>(nibbles - skewbytes));
  memcpy(trackImageBuffer + static_cast<size_t>(nibbles - skewbytes),  workbuffer.data(),  static_cast<size_t>(skewbytes));
}

// RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION

auto AplBoot(imageinfoptr ptr) -> bool {
  fseek(ptr->file.get(), 0, SEEK_SET);
  uint16_t address = 0;
  uint16_t length = 0;
  fread(&address, 1, sizeof(uint16_t), ptr->file.get());
  fread(&length, 1, sizeof(uint16_t), ptr->file.get());
  if (((static_cast<uint32_t>(address + length)) <= address) || (address >= IO_REGION_START) || (address + length - 1 >= IO_REGION_START)) {
    return false;
  }
  fread(mem + address, 1, length, ptr->file.get());
  int loop = NUM_PAGES_48K;
  while (loop--) {
    memdirty[loop] = 0xFF;
  }
  regs.pc = address;
  return true;
}

auto AplDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t {
  uint32_t length = *reinterpret_cast<uint16_t*>(imageptr + 2);
  return (((length + 4) == imagesize) || ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == imagesize)) ? 2 : 0;
}

// DOS ORDER (DO) FORMAT IMPLEMENTATION
auto DoDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t {
  if (((imagesize < MIN_140K_DISK_SIZE) || (imagesize > MAX_140K_DISK_SIZE)) && (imagesize != DISK_SIZE_140K_ALT1) && (imagesize != DISK_SIZE_140K_ALT2)) {
    return 0;
  }

  // Check for a DOS order image of a DOS diskette
  {
    int loop = 0;
    bool mismatch = false;
    while ((loop++ < 15) && !mismatch) {
      if (*(imageptr + VTOC_OFFSET + 2 + (loop * PAGE_SIZE)) != loop - 1) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  // Check for a DOS order image of a PRODOS diskette
  {
    int loop = 1;
    bool mismatch = false;
    while ((loop++ < 5) && !mismatch) {
      if ((*reinterpret_cast<uint16_t*>(imageptr + (loop * PRODOS_BLOCK_SIZE) + PAGE_SIZE) != ((loop == 5) ? 0 : 6 - loop)) ||
          (*reinterpret_cast<uint16_t*>(imageptr + (loop * PRODOS_BLOCK_SIZE) + PAGE_SIZE + 2) != ((loop == 2) ? 0 : 8 - loop))) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }
  return 1;
}

void DoRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles) {
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  memset(workbuffer.data(), 0, DOS_TRACK_SIZE);
  fread(workbuffer.data(), 1, DOS_TRACK_SIZE, ptr->file.get());
  *nibbles = static_cast<int>(NibblizeTrack(trackImageBuffer, true, track));
  if (!enhancedisk) {
    SkewTrack(track, *nibbles, trackImageBuffer);
  }
}

void DoWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles)
{
  (void)quartertrack;
  memset(workbuffer.data(), 0, DOS_TRACK_SIZE);
  DenibblizeTrack(trackimage, true, nibbles);
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fwrite(workbuffer.data(), 1, DOS_TRACK_SIZE, ptr->file.get());
}

// SIMSYSTEM IIE (IIE) format implementation

const int IIE_HEADER_SIZE = 88;
const int IIE_TRACK_DATA_OFFSET = 30;
const char IIE_SIGNATURE[] = "SIMSYSTEM_IIE";
const int IIE_SIGNATURE_LEN = 13;

void IieConvertSectorOrder(uint8_t* sourceorder) {
  int loop = SECTORS_PER_TRACK_16;
  while (loop--) {
    uint8_t found = 0xFF;
    int loop2 = SECTORS_PER_TRACK_16;
    while (loop2-- && (found == 0xFF)) {
      if (*(sourceorder + loop2) == loop) {
        found = static_cast<uint8_t>(loop2);
      }
    }
    if (found == 0xFF) {
      found = 0;
    }
    sectornumber[2][loop] = found;
  }
}

auto IieDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t
{
  (void)imagesize;
  if (strncmp(reinterpret_cast<const char *>(imageptr), IIE_SIGNATURE, IIE_SIGNATURE_LEN) || (*(imageptr + IIE_SIGNATURE_LEN) > 3)) {
    return 0;
  }
  return 2;
}

void IieRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles)
{
  (void)quartertrack;
  // If we haven't already done so, read the image file header
  if (!ptr->header) {
    ptr->header.reset(static_cast<uint8_t*>(malloc(IIE_HEADER_SIZE)));
    if (!ptr->header) {
      *nibbles = 0;
      return;
    }
    memset(ptr->header.get(), 0, IIE_HEADER_SIZE);
    fseek(ptr->file.get(), 0, SEEK_SET);
    fread(ptr->header.get(), 1, IIE_HEADER_SIZE, ptr->file.get());
  }

  if (ptr->header[IIE_SIGNATURE_LEN] <= 2) {
    // If this image contains user data, read the track and nibblize it
    IieConvertSectorOrder(&ptr->header[14]);
    fseek(ptr->file.get(), (track * DOS_TRACK_SIZE) + IIE_TRACK_DATA_OFFSET, SEEK_SET);
    memset(workbuffer.data(), 0, DOS_TRACK_SIZE);
    fread(workbuffer.data(), 1, DOS_TRACK_SIZE, ptr->file.get());
    *nibbles = static_cast<int>(NibblizeTrack(trackImageBuffer, true, track));
  } else {
    // Otherwise, if this image contains nibble information, read it directly into the track buffer
    *nibbles = *reinterpret_cast<uint16_t*>(&ptr->header[(track << 1) + 14]);
    uint32_t offset = IIE_HEADER_SIZE;
    int tempTrack = track;
    while (tempTrack--) {
      offset += *reinterpret_cast<uint16_t*>(&ptr->header[(tempTrack << 1) + 14]);
    }
    fseek(ptr->file.get(), static_cast<long>(offset), SEEK_SET);
    memset(trackImageBuffer, 0, static_cast<size_t>(*nibbles));
    fread(trackImageBuffer, 1, static_cast<size_t>(*nibbles), ptr->file.get());
  }
}

void IieWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles)
{
  (void)ptr;
  (void)track;
  (void)quartertrack;
  (void)trackimage;
  (void)nibbles;
}

// Nibblized 6656-nibble (nib) format implementation

auto Nib1Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t
{
  (void)imageptr;
  return (imagesize == DISK_SIZE_NIB) ? 2 : 0;
}

void Nib1Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles)
{
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + static_cast<uint32_t>(track * NIBBLES_PER_TRACK)), SEEK_SET);
  *nibbles = static_cast<int>(fread(trackImageBuffer, 1, NIBBLES_PER_TRACK, ptr->file.get()));
}

void Nib1Write(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles)
{
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + static_cast<uint32_t>(track * NIBBLES_PER_TRACK)), SEEK_SET);
  fwrite(trackimage, 1, static_cast<size_t>(nibbles), ptr->file.get());
}

// NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION

auto Nib2Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t
{
  (void)imageptr;
  return (imagesize == DISK_SIZE_NB2) ? 2 : 0;
}

void Nib2Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles)
{
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + static_cast<uint32_t>(track * NIBBLES_PER_TRACK_NB2)), SEEK_SET);
  *nibbles = static_cast<int>(fread(trackImageBuffer, 1, NIBBLES_PER_TRACK_NB2, ptr->file.get()));
}

void Nib2Write(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles)
{
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + static_cast<uint32_t>(track * NIBBLES_PER_TRACK_NB2)), SEEK_SET);
  fwrite(trackimage, 1, static_cast<size_t>(nibbles), ptr->file.get());
}

// PRODOS order (po) format implementation

auto PoDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t {
  if (((imagesize < MIN_140K_DISK_SIZE) || (imagesize > MAX_140K_DISK_SIZE)) && (imagesize != DISK_SIZE_140K_ALT2)) {
    return 0;
  }

  // Check for a PRODOS order image of a dos diskette
  {
    int loop = 4;
    bool mismatch = false;
    while ((loop++ < 13) && !mismatch) {
      if (*(imageptr + VTOC_OFFSET + 2 + (loop * PAGE_SIZE)) != 14 - loop) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  // Check for a PRODOS order image of a prodos diskette
  {
    int loop = 1;
    bool mismatch = false;
    while ((loop++ < 5) && !mismatch) {
      if ((*reinterpret_cast<uint16_t*>(imageptr + (loop * PRODOS_BLOCK_SIZE)) != ((loop == 2) ? 0 : loop - 1)) ||
          (*reinterpret_cast<uint16_t*>(imageptr + (loop * PRODOS_BLOCK_SIZE) + 2) != ((loop == 5) ? 0 : loop + 1))) {
        mismatch = true;
      }
    }
    if (!mismatch) {
      return 2;
    }
  }

  return 1;
}

void PoRead(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles)
{
  (void)quartertrack;
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  memset(workbuffer.data(), 0, DOS_TRACK_SIZE);
  fread(workbuffer.data(), 1, DOS_TRACK_SIZE, ptr->file.get());
  *nibbles = static_cast<int>(NibblizeTrack(trackImageBuffer, false, track));
  if (!enhancedisk) {
    SkewTrack(track, *nibbles, trackImageBuffer);
  }
}

void PoWrite(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackimage, int nibbles)
{
  (void)quartertrack;
  memset(workbuffer.data(), 0, DOS_TRACK_SIZE);
  DenibblizeTrack(trackimage, false, nibbles);
  fseek(ptr->file.get(), static_cast<long>(ptr->offset + (track * DOS_TRACK_SIZE)), SEEK_SET);
  fwrite(workbuffer.data(), 1, DOS_TRACK_SIZE, ptr->file.get());
}

// PRODOS PROGRAM IMAGE (PRG) FORMAT IMPLEMENTATION

const int PRG_HEADER_INFO_OFFSET = 5;
const int PRG_DATA_OFFSET = 128;
const uint32_t PRG_MAGIC_NUMBER = 0x214C470A;

auto PrgBoot(imageinfoptr ptr) -> bool
{
  fseek(ptr->file.get(), PRG_HEADER_INFO_OFFSET, SEEK_SET);
  uint16_t address = 0;
  uint16_t length = 0;
  fread(&address, 1, sizeof(uint16_t), ptr->file.get());
  fread(&length, 1, sizeof(uint16_t), ptr->file.get());
  length <<= 1;
  if (((static_cast<uint32_t>(address + length)) <= address) || (address >= IO_REGION_START) || (address + length - 1 >= IO_REGION_START)) {
    return false;
  }
  fseek(ptr->file.get(), PRG_DATA_OFFSET, SEEK_SET);
  fread(mem + address, 1, length, ptr->file.get());
  int loop = NUM_PAGES_48K;
  while (loop--) {
    memdirty[loop] = 0xFF;
  }
  regs.pc = address;
  return true;
}

auto PrgDetect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t
{
  (void)imagesize;
  return (*reinterpret_cast<uint32_t*>(imageptr) == PRG_MAGIC_NUMBER) ? 2 : 0;
}

// WOZ2 (woz) format implementation
// see: https://applesaucefdc.com/woz/reference2/

const int WOZ2_SIGNATURE_LEN = 8;
const int WOZ2_QUARTER_TRACKS_PER_TRACK = 4;
const uint8_t WOZ2_UNRECORDED_TRACK = 0xFF;
const int WOZ2_SYNC_BYTES_16SECTOR = 4;
const int WOZ2_TRAILING_ZEROS_16SECTOR = 2;
const int WOZ2_SYNC_BYTES_13SECTOR = 7;

auto Woz2Detect(uint8_t* imageptr, uint32_t imagesize) -> uint32_t
{
  if (strncmp(reinterpret_cast<const char *>(imageptr), "WOZ2\xFF\n\r\n", WOZ2_SIGNATURE_LEN) != 0) {
    return 0;
  }
  if (imagesize < WOZ2_HEADER_SIZE) {
    return 0;
  }
  return 2;
}

static auto woz2_scan_sync_bytes(const uint8_t* buffer,
                                         const uint32_t bit_count,
                                         const uint32_t sync_bytes_needed,
                                         const uint32_t trailing_0_count) -> uint32_t;

void Woz2Read(imageinfoptr ptr, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles)
{
  (void)quartertrack;
  uint32_t bytesRead = 0;
  if (!ptr->header) {
    ptr->header.reset(static_cast<uint8_t*>(malloc(WOZ2_HEADER_SIZE)));
    if (!ptr->header) {
      *nibbles = 0;
      return;
    }
    memset(ptr->header.get(), 0, WOZ2_HEADER_SIZE);
    fseek(ptr->file.get(), static_cast<long>(ptr->offset), SEEK_SET);
    bytesRead = static_cast<uint32_t>(fread(ptr->header.get(), 1, WOZ2_HEADER_SIZE, ptr->file.get()));
    assert(bytesRead == WOZ2_HEADER_SIZE);
  }

  uint8_t* tmap = ptr->header.get() + WOZ2_TMAP_OFFSET;
  uint8_t* trks = ptr->header.get() + WOZ2_TRKS_OFFSET;
  uint8_t* trk = nullptr;

  // Sorry, only integral tracks supported.
  // This is because the Disk II emulation does not properly handle
  // track data for half-/quarter- tracks.
  const uint32_t tmap_index = WOZ2_QUARTER_TRACKS_PER_TRACK * static_cast<uint32_t>(track);
  assert(tmap_index <= WOZ2_TMAP_SIZE);

  const uint32_t trks_index = tmap[tmap_index];

  if (trks_index == WOZ2_UNRECORDED_TRACK) { // unrecorded track ==> random data
    for (uint32_t i = 0; i < NIBBLES_PER_TRACK; ++i) {
      trackImageBuffer[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
    *nibbles = static_cast<int>(NIBBLES_PER_TRACK);
    return;
  }

  assert(trks_index <= WOZ2_TRKS_MAX_SIZE);
  trk = trks + static_cast<size_t>(trks_index * WOZ2_TRK_SIZE);

  const uint16_t starting_block = static_cast<uint16_t>(trk[0] | (static_cast<uint16_t>(trk[1]))<<8);
  const uint16_t block_count = static_cast<uint16_t>(trk[2] | (static_cast<uint16_t>(trk[3]))<<8);
  const uint32_t bit_count =  trk[4] | (static_cast<uint16_t>(trk[5]))<<8
    | (static_cast<uint16_t>(trk[6]))<<16 | (static_cast<uint16_t>(trk[7]))<<24;

  fseek(ptr->file.get(), static_cast<long>(ptr->offset + static_cast<uint32_t>(starting_block * WOZ2_DATA_BLOCK_SIZE)), SEEK_SET);

  const uint32_t byte_count = block_count * WOZ2_DATA_BLOCK_SIZE;
  std::unique_ptr<uint8_t[], void(*)(void*)> buffer(static_cast<uint8_t*>(malloc(byte_count)), free);
  if (buffer) memset(buffer.get(), 0, byte_count);
  if (!buffer) {
    *nibbles = 0;
    return;
  }

  bytesRead = static_cast<uint32_t>(fread(buffer.get(), 1, byte_count, ptr->file.get()));
  assert(bytesRead == byte_count);


  // scan for sync bytes
  // try 16-sector format first...
  uint32_t i = woz2_scan_sync_bytes(buffer.get(), bit_count,
                                        WOZ2_SYNC_BYTES_16SECTOR, // 4 sync bytes are enough
                                        WOZ2_TRAILING_ZEROS_16SECTOR);
  if (i == 0) { // sync bytes not found: try 13-sector format...
    i = woz2_scan_sync_bytes(buffer.get(), bit_count,
                             WOZ2_SYNC_BYTES_13SECTOR, // need 7 sync bytes
                             0); // and no trailing 0 bits (for Disk ][ 13-sector format)
  }
  if (i == 0) { // sync bytes (still) not found
    *nibbles = 0;
    return;
  }


  // now, scan for nibbles
#define FETCH_BIT(i) ((buffer.get()[(i)/BITS_PER_BYTE] & (0x80 >> ((i)%BITS_PER_BYTE))) == 0? 0 : 1)
#define WRAP_BIT_INDEX(i)  do{ if ((i) >= bit_count) (i) -= bit_count; }while(0)
  uint32_t bits_left = bit_count;
  uint32_t nibbles_done = 0;
  WRAP_BIT_INDEX(i);
  while (nibbles_done < NIBBLES_PER_TRACK && bits_left > 0) {
    while (bits_left > 0) {
      if (FETCH_BIT(i) == 1) {
        break;
}
      ++i;
      WRAP_BIT_INDEX(i);
      --bits_left;
    }
    if (bits_left < 8) {
      break;
    }

    uint8_t nibble = 0;
    for (int b=0; b<8; ++b) {
      nibble = static_cast<uint8_t>((nibble << 1) | FETCH_BIT(i));
      ++i;
      WRAP_BIT_INDEX(i);
    }
    bits_left -= 8;

    trackImageBuffer[nibbles_done++] = nibble;
  }

  *nibbles = static_cast<int>(nibbles_done);
}

#undef FETCH_BIT
#undef WRAP_BIT_INDEX

static auto woz2_scan_sync_bytes(const uint8_t* buffer,
                                         const uint32_t bit_count,
                                         const uint32_t sync_bytes_needed,
                                         const uint32_t trailing_0_count) -> uint32_t
{
#define FETCH_BIT(i) ((buffer[(i)/BITS_PER_BYTE] & (0x80 >> ((i)%BITS_PER_BYTE))) == 0? 0 : 1)

  uint32_t i = 0;
  bool found_sync_FFs = false;

 rescan: for (;;) {
    while (i < bit_count && FETCH_BIT(i)==0) {
      ++i;
    }
    if (i >= bit_count) {
      break;
    }

    uint32_t nr_FFs = 0;
    for (;;) {
      uint32_t nr_1s = 0;
      while (i < bit_count && FETCH_BIT(i)==1) {
        ++nr_1s;
        ++i;
      }
      if (nr_1s < 8) {
        goto rescan;
      }
      if (nr_1s > 8) {
        nr_FFs = 1; // treat it as first FF
      }

      uint32_t nr_0s = 0;
      while (i < bit_count && FETCH_BIT(i)==0) {
        ++nr_0s;
        ++i;
      }
      if (nr_0s != trailing_0_count) {
        goto rescan;
      }

      // we've found 1111_1111_0[0].  This is an autosync byte
      ++nr_FFs;
      if (nr_FFs == sync_bytes_needed) {
        found_sync_FFs = true;
        break;
      }
    }
    if (found_sync_FFs) {
      break;
    }
  }

  if (!found_sync_FFs) {
    return 0; // 0 means not found
  }

  return i;
}

#undef FETCH_BIT

// All globally accessible functions are below this line

auto ImageBoot(HIMAGE imageHandle) -> bool {
  auto ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  bool result = false;
  if (imagetype[ptr->format].boot) {
    result = imagetype[ptr->format].boot(ptr);
  }
  if (result) {
    ptr->writeProtected = true;
  }
  return result;
}

void ImageClose(HIMAGE imageHandle)
{
  std::unique_ptr<imageinforec> ptr(reinterpret_cast<imageinfoptr>(imageHandle));
  if (ptr->file) {
    ptr->file.reset();
  }
  for (bool track : ptr->validTrack) {
    if (!track) {
      remove(ptr->filename);
      break;
    }
  }
  // ptr->header and ptr itself will be freed by unique_ptr
}

void ImageDestroy()
{
  workbuffer.clear();
}

void ImageInitialize()
{
  const uint32_t GCR_WORK_BUFFER_SIZE = 0x2000;
  workbuffer.resize(GCR_WORK_BUFFER_SIZE, 0);
}

const int MACBINARY_HEADER_SIZE = 128;
const int MACBINARY_FILENAME_MAX = 120;
const int MACBINARY_MAGIC_OFFSET1 = 0x7A;
const int MACBINARY_MAGIC_OFFSET2 = 0x7B;
const uint8_t MACBINARY_MAGIC_VALUE = 0x81;

auto ImageOpen(const char* imagefilename, HIMAGE *hDiskImage_, bool *pWriteProtected_, bool bCreateIfNecessary) -> int
{
  if (!(imagefilename && hDiskImage_ && pWriteProtected_ && !workbuffer.empty())) {
    return IMAGE_ERROR_BAD_POINTER;
  } // HACK: MAGIC # -1

  // Try to open the image file
  FilePtr file(nullptr, fclose);

  if (!*pWriteProtected_) {
    file.reset(fopen(imagefilename, "r+b")); // open file in r/w mode
}
  // File may have read-only attribute set, so try to open as read-only.
  if (!file) {
    file.reset(fopen(imagefilename, "rb")); // open file just for reading

    if (file) {
      *pWriteProtected_ = true;
}
  }

  if ((!file) && bCreateIfNecessary) {
    file.reset(fopen(imagefilename, "a+b")); // create file
  }

  // If we aren't able to open the file, return
  if (!file) {
    return IMAGE_ERROR_UNABLE_TO_OPEN; // HACK: MAGIC # 1
  }

  // Determine the file's extension and convert it to lowercase
  const char* imagefileext = imagefilename;
  if (strrchr(imagefileext, FILE_SEPARATOR)) {
    imagefileext = strrchr(imagefileext, FILE_SEPARATOR) + 1;
  }
  if (strrchr(imagefileext, '.')) {
    imagefileext = strrchr(imagefileext, '.');
  }

  #define MAX_EXT  5
  char ext[MAX_EXT];

  Util_SafeStrCpy(ext, imagefileext, MAX_EXT);

  for (char* p = ext; *p; ++p) *p = static_cast<char>(tolower(static_cast<uint8_t>(*p)));

  uint32_t size = static_cast<uint32_t>(Util_GetFileSize(file.get()));
  std::unique_ptr<uint8_t[], void(*)(void*)> view(nullptr, free);
  uint8_t* pImage = nullptr;

  const uint32_t UNKNOWN_FORMAT = 0xFFFFFFFF;
  uint32_t format = UNKNOWN_FORMAT;

  if (size > 0) {
    view.reset(static_cast<uint8_t*>(malloc(size)));
    if (view) {
      size_t bytesRead = fread(view.get(), 1, size, file.get());
      if (bytesRead > 0) {
        fseek(file.get(), 0, SEEK_SET); // I just got accustomed to mrsftish FILE_BEGIN, FILE_END, etc. Hmm. ^_^
      }
      pImage = view.get();

      // Determine whether the file has a 128-byte macbinary header
      if ((size > MACBINARY_HEADER_SIZE) && (!*pImage) && (*(pImage + 1) < MACBINARY_FILENAME_MAX) && (!*(pImage + *(pImage + 1) + 2)) &&
          (*(pImage + MACBINARY_MAGIC_OFFSET1) == MACBINARY_MAGIC_VALUE) && (*(pImage + MACBINARY_MAGIC_OFFSET2) == MACBINARY_MAGIC_VALUE)) {
        pImage += MACBINARY_HEADER_SIZE;
        size -= MACBINARY_HEADER_SIZE;
      }

      // Call the detection functions in order, looking for a match
      uint32_t possibleformat = UNKNOWN_FORMAT;
      int loop = 0;
      while ((loop < IMAGETYPES) && (format == UNKNOWN_FORMAT)) {
        if (*ext && strstr(imagetype[loop].rejectExts, ext)) {
          ++loop;
        } else {
          uint32_t result = imagetype[loop].detect(pImage, size);
          if (result == 2) {
            format = loop;
          } else if ((result == 1) && (possibleformat == UNKNOWN_FORMAT)) {
            possibleformat = loop++;
          } else {
            ++loop;
          }
        }
      }

      if (format == UNKNOWN_FORMAT) {
        format = possibleformat;
}
    }
  } else {
    // We create only DOS order (do) or 6656-nibble (nib) format files
    static const ImageTypeIndex_e create_formats[] = { IMG_TYPE_DO, IMG_TYPE_NIB };
    for (ImageTypeIndex_e loop : create_formats) {
      if (*ext && strstr(imagetype[static_cast<int>(loop)].createExts, ext)) {
        format = static_cast<uint32_t>(loop);
        break;
      }
    }
  }

  // If the file does match a known format
  if (format != UNKNOWN_FORMAT) {
    // Create a record for the file, and return an image handle
    auto ptr = std::unique_ptr<imageinforec>(new imageinforec());
    if (ptr) {
      // Do this in DiskInsert
      Util_SafeStrCpy(ptr->filename, imagefilename, MAX_PATH);
      ptr->format = format;
      ptr->offset = static_cast<uint32_t>(pImage - view.get());
      ptr->writeProtected = *pWriteProtected_;
      ptr->file = std::move(file);

      for (bool & track : ptr->validTrack) {
        track = (size > 0);
      }

      *hDiskImage_ = reinterpret_cast<HIMAGE>(ptr.release());
      return IMAGE_ERROR_NONE; // HACK: MAGIC # 0
    }
  }

  if (size <= 0) {
    remove(imagefilename);
  }

  return IMAGE_ERROR_BAD_SIZE; // HACK: MAGIC # 2
}

void ImageReadTrack(HIMAGE imageHandle, int track, int quartertrack, uint8_t* trackImageBuffer, int *nibbles) {
  auto ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  if (imagetype[ptr->format].read && ptr->validTrack[track]) {
    imagetype[ptr->format].read(ptr, track, quartertrack, trackImageBuffer, nibbles);
  } else {
    for (uint32_t i = 0; i < NIBBLES_PER_TRACK; i++) {
      trackImageBuffer[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
    *nibbles = static_cast<int>(NIBBLES_PER_TRACK);
  }
}

void ImageWriteTrack(HIMAGE imageHandle, int track, int quartertrack, uint8_t* trackimage, int nibbles) {
  auto ptr = reinterpret_cast<imageinfoptr>(imageHandle);
  if (imagetype[ptr->format].write && !ptr->writeProtected) {
    imagetype[ptr->format].write(ptr, track, quartertrack, trackimage, nibbles);
    ptr->validTrack[track] = true;
  }
}
