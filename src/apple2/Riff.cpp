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

/* Description: RIFF funcs  --- my GOD, what is RIFF?
 *
 * Author: Various
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "core/Common.h"
#include "apple2/Riff.h"

static FilePtr g_hRiffFile(nullptr, fclose);
static uint32_t dwTotalOffset;
static uint32_t dwDataOffset;
static uint32_t g_dwTotalNumberOfBytesWritten = 0;
static uint32_t g_NumChannels = 2;

auto RiffInitWriteFile(char *pszFile, uint32_t sample_rate, uint32_t NumChannels) -> int
{
  g_hRiffFile.reset(fopen(pszFile, "wb"));

  if (!g_hRiffFile) {
    return 1;
  }

  g_NumChannels = NumChannels;

  uint32_t temp32 = 0;
  uint16_t temp16 = 0;

  fwrite("RIFF", 1, 4, g_hRiffFile.get());

  temp32 = 0;        // total size
  dwTotalOffset = static_cast<uint32_t>(ftell(g_hRiffFile.get()));
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  fwrite("WAVE", 1, 4, g_hRiffFile.get());

  fwrite("fmt ", 1, 4, g_hRiffFile.get());

  temp32 = 16;      // format length
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  temp16 = 1;        // PCM format
  fwrite(&temp16, 1, 2, g_hRiffFile.get());

  temp16 = static_cast<uint16_t>(NumChannels);    // channels
  fwrite(&temp16, 1, 2, g_hRiffFile.get());

  temp32 = sample_rate;  // sample rate
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  temp32 = sample_rate * 2 * NumChannels;  // bytes/second
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  temp16 = static_cast<uint16_t>(2 * NumChannels);  // block align
  fwrite(&temp16, 1, 2, g_hRiffFile.get());

  temp16 = 16;      // bits/sample
  fwrite(&temp16, 1, 2, g_hRiffFile.get());

  fwrite("data", 1, 4, g_hRiffFile.get());

  temp32 = 0;        // data length
  dwDataOffset = static_cast<uint32_t>(ftell(g_hRiffFile.get()));
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  g_dwTotalNumberOfBytesWritten = static_cast<uint32_t>(ftell(g_hRiffFile.get()));

  return 0;
}

auto RiffFinishWriteFile() -> int {
  if (!g_hRiffFile) {
    return 1;
  }

  uint32_t temp32 = 0;

  temp32 = g_dwTotalNumberOfBytesWritten - (dwTotalOffset + 4);
  fseek(g_hRiffFile.get(), static_cast<long>(dwTotalOffset), SEEK_SET);
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  temp32 = g_dwTotalNumberOfBytesWritten - (dwDataOffset + 4);
  fseek(g_hRiffFile.get(), static_cast<long>(dwDataOffset), SEEK_SET);
  fwrite(&temp32, 1, 4, g_hRiffFile.get());

  g_hRiffFile.reset();
  return 0;
}

auto RiffPutSamples(short *buf, uint32_t uSamples) -> int {
  if (!g_hRiffFile) {
    return 1;
  }

  size_t bytesToWrite = static_cast<size_t>(uSamples) * sizeof(short) * g_NumChannels;
  size_t bytesWritten = fwrite(buf, 1, bytesToWrite, g_hRiffFile.get());
  g_dwTotalNumberOfBytesWritten += static_cast<uint32_t>(bytesWritten);

  return 0;
}
