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

#include "core/Common.h"
#include <iostream>
#include <cstring>
#include <array>
#include "apple2/Keyboard.h"
#include "apple2/Structs.h"
#include "apple2/CPU.h"
#include "core/Common_Globals.h"
#include "core/Log.h"

extern void FrameRefreshStatus(int);

bool g_bShiftKey = false;
bool g_bCtrlKey = false;
bool g_bAltKey = false;
bool g_bAltGrKey = false;

static bool g_bCapsLock = true;
static uint32_t keyboardqueries = 0;

KeybLanguage g_KeyboardLanguage = English_US;
bool         g_KeyboardRockerSwitch = false;
static bool  g_bKeybBufferEnable = true;

const int KEY_BUFFER_MIN_SIZE = 1;
const int KEY_BUFFER_MAX_SIZE = 16;
static int g_nKeyBufferSize = KEY_BUFFER_MAX_SIZE;
static int g_nNextInIdx = 0;
static int g_nNextOutIdx = 0;
static int g_nKeyBufferCnt = 0;

struct KeyBufferEntry_t {
  uint8_t nAppleKey;
  uint64_t nTimestamp;
};

static std::array<KeyBufferEntry_t, KEY_BUFFER_MAX_SIZE> g_nKeyBuffer;

static uint8_t g_nLastKey = 0x00;
static bool g_bAnyKeyDown = false;
static int g_nKeysDownCount = 0;

void KeybReset() {
  g_nNextInIdx = 0;
  g_nNextOutIdx = 0;
  g_nKeyBufferCnt = 0;
  g_nLastKey = 0x00;
  g_bAnyKeyDown = false;
  g_nKeysDownCount = 0;
  g_nKeyBufferSize = g_bKeybBufferEnable ? KEY_BUFFER_MAX_SIZE : KEY_BUFFER_MIN_SIZE;
}

void KeybSetAnyKeyDownStatus(bool bDown) {
  if (bDown) {
    g_nKeysDownCount++;
  } else {
    if (g_nKeysDownCount > 0) g_nKeysDownCount--;
  }
  g_bAnyKeyDown = (g_nKeysDownCount > 0);
}

auto KeybGetAnyKeyDownStatus() -> bool {
  return g_bAnyKeyDown;
}

void KeybSetModifiers(bool bShift, bool bCtrl, bool bAlt) {
  g_bShiftKey = bShift;
  g_bCtrlKey = bCtrl;
  g_bAltKey = bAlt;
}

void KeybPushAppleKey(uint8_t apple_code) {
  bool bOverflow = false;
  if (g_nKeyBufferCnt < g_nKeyBufferSize) {
    g_nKeyBufferCnt++;
  } else {
    bOverflow = true;
  }

  g_nKeyBuffer[g_nNextInIdx].nAppleKey = apple_code;
  g_nKeyBuffer[g_nNextInIdx].nTimestamp = 0;
  g_nNextInIdx = (g_nNextInIdx + 1) % g_nKeyBufferSize;

  if (bOverflow) {
    g_nNextOutIdx = (g_nNextOutIdx + 1) % g_nKeyBufferSize;
  }
}

auto KeybGetAltStatus() -> bool { return g_bAltKey; }
auto KeybGetCapsStatus() -> bool { return g_bCapsLock; }
auto KeybGetCtrlStatus() -> bool { return g_bCtrlKey; }
auto KeybGetShiftStatus() -> bool { return g_bShiftKey; }

void KeybUpdateCtrlShiftStatus() {
}

auto KeybGetKeycode() -> uint8_t {
  // Returns the latest key from the buffer without clearing it
  if (g_nKeyBufferCnt) return g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
  return g_nLastKey;
}

auto KeybGetNumQueries() -> uint32_t {
  uint32_t result = keyboardqueries;
  keyboardqueries = 0;
  return result;
}

auto KeybReadData(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t) -> uint8_t {
  keyboardqueries++;
  uint8_t nKey = g_nKeyBufferCnt ? 0x80 : 0;
  if (g_nKeyBufferCnt) {
    nKey |= g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
    g_nLastKey = g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
    g_nKeyBuffer[g_nNextOutIdx].nTimestamp = 0;
  } else {
    nKey |= g_nLastKey;
  }
  return nKey;
}

auto KeybReadFlag(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t) -> uint8_t {
  keyboardqueries++;
  uint8_t nKey = g_nLastKey;
  if (!IS_APPLE2()) {
    if (g_bAnyKeyDown) nKey |= 0x80;
  }
  return nKey;
}

auto KeybClearFlag(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t) -> uint8_t {
  g_nLastKey &= 0x7F;
  if (g_nKeyBufferCnt) {
    g_nKeyBufferCnt--;
    g_nNextOutIdx = (g_nNextOutIdx + 1) % g_nKeyBufferSize;
    if (g_nKeyBufferCnt) {
        g_nLastKey = g_nKeyBuffer[g_nNextOutIdx].nAppleKey | 0x80;
    }
  }
  return g_nLastKey;
}

void KeybSetCapsLock(bool bState) {
  if (!IS_APPLE2()) {
    if (g_bCapsLock != bState) {
      g_bCapsLock = bState;
      FrameRefreshStatus(DRAW_LEDS);
    }
  }
}

auto KeybGetSnapshot(SS_IO_Keyboard *pSS) -> uint32_t {
  pSS->keyboardqueries = keyboardqueries;
  pSS->nLastKey = g_nLastKey;
  return 0;
}

auto KeybSetSnapshot(SS_IO_Keyboard *pSS) -> uint32_t {
  keyboardqueries = pSS->keyboardqueries;
  g_nLastKey = pSS->nLastKey;
  return 0;
}

void KeybQueueKeypress(uint8_t apple_code) {
  KeybPushAppleKey(apple_code);
}

void ClipboardInitiatePaste() {
  // To be implemented
}
