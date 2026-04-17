#include "core/Common.h"
#include <cstring>
#include "apple2/Speaker.h"
#include "apple2/Structs.h"
#include "apple2/CPU.h"
#include "apple2/Memory.h"
#include "core/Common_Globals.h"
#include "core/Log.h"
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

/* Description: Speaker hardware emulation (Core)
 *
 * This module tracks cycle-exact speaker toggles.
 * Sample generation and SDL audio are handled by the frontend.
 */

static SpkrEvent g_spkrEvents[MAX_SPKR_EVENTS];
static int g_nNumSpkrEvents = 0;
static bool g_bSpkrState = false;
static uint64_t g_nSpkrLastCycle = 0;
static uint64_t g_nSpkrQuietCycleCount = 0;
static bool g_bSpkrRecentlyActive = false;
static bool g_bSpkrToggleFlag = false;

uint32_t soundtype = SOUND_WAVE;

void SpkrDestroy() {}

void SpkrInitialize() {
  g_nNumSpkrEvents = 0;
  g_bSpkrState = false;
  g_nSpkrLastCycle = g_nCumulativeCycles;
  g_nSpkrQuietCycleCount = 0;
  g_bSpkrRecentlyActive = false;
  g_bSpkrToggleFlag = false;
}

void SpkrReinitialize() {}

void SpkrReset() {
  g_nNumSpkrEvents = 0;
  g_bSpkrState = false;
  g_nSpkrLastCycle = g_nCumulativeCycles;
  g_nSpkrQuietCycleCount = 0;
  g_bSpkrRecentlyActive = false;
  g_bSpkrToggleFlag = false;
}

static void Spkr_SetActive(bool bActive) {
  g_bSpkrRecentlyActive = bActive;
}

auto Spkr_IsActive() -> bool {
  return g_bSpkrRecentlyActive;
}

auto SpkrToggle(void* instance, uint16_t, uint16_t, uint8_t, uint8_t, uint32_t nCyclesLeft) -> uint8_t {
  (void)instance;
  g_bSpkrToggleFlag = true;

  if (!g_bFullSpeed) {
    Spkr_SetActive(true);
  }

  // Record toggle event
  if (soundtype == SOUND_WAVE && g_nNumSpkrEvents < MAX_SPKR_EVENTS) {
    g_spkrEvents[g_nNumSpkrEvents].cycle = g_nCumulativeCycles;
    g_spkrEvents[g_nNumSpkrEvents].state = g_bSpkrState = !g_bSpkrState;
    g_nNumSpkrEvents++;
  }

  return MemReadFloatingBus(nCyclesLeft);
}

#include "core/Peripheral.h"

// Justification: Peripheral Host Interface requires storage of core callback 
// services and active slot information for the migrated Speaker instance.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static HostInterface_t* g_pSpkrHost = nullptr;

static constexpr uint16_t ADDR_SPEAKER = 0xC030;

static auto Spkr_ABI_Init(int slot, HostInterface_t* host) -> void* {
  (void)slot;
  g_pSpkrHost = host;
  SpkrInitialize();
  
  // Speaker is at $C030
  g_pSpkrHost->RegisterDirectIO(nullptr, ADDR_SPEAKER, SpkrToggle, SpkrToggle);
  
  return reinterpret_cast<void*>(1); // Dummy instance
}

static void Spkr_ABI_Reset(void* instance) {
  (void)instance;
  SpkrReset();
}

static void Spkr_ABI_Shutdown(void* instance) {
  (void)instance;
  SpkrDestroy();
}

static void Spkr_ABI_Think(void* instance, uint32_t cycles) {
  (void)instance;
  SpkrUpdate(cycles);
}

static auto Spkr_ABI_SaveState(void* instance, void* buffer, size_t* size) -> PeripheralStatus {
  (void)instance;
  if (!buffer || !size || *size < sizeof(SS_IO_Speaker)) {
    if (size) *size = sizeof(SS_IO_Speaker);
    return PERIPHERAL_ERROR;
  }
  SpkrGetSnapshot(static_cast<SS_IO_Speaker*>(buffer));
  *size = sizeof(SS_IO_Speaker);
  return PERIPHERAL_OK;
}

static auto Spkr_ABI_LoadState(void* instance, const void* buffer, size_t size) -> PeripheralStatus {
  (void)instance;
  if (!buffer || size < sizeof(SS_IO_Speaker)) {
    return PERIPHERAL_ERROR;
  }
  SpkrSetSnapshot(const_cast<SS_IO_Speaker*>(static_cast<const SS_IO_Speaker*>(buffer)));
  return PERIPHERAL_OK;
}

Peripheral_t g_speaker_peripheral = {
    LINAPPLE_ABI_VERSION,
    "Speaker",
    LINAPPLE_ANY_SLOT_MASK, // Compatible with any "slot"
    Spkr_ABI_Init,
    Spkr_ABI_Reset,
    Spkr_ABI_Shutdown,
    Spkr_ABI_Think,
    nullptr, // on_vblank
    Spkr_ABI_SaveState,
    Spkr_ABI_LoadState,
    nullptr, // command
    nullptr  // query
};

#ifdef BUILD_SHARED_PERIPHERAL
EXPORT_PERIPHERAL(g_speaker_peripheral)
#endif
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void SpkrUpdate(uint32_t totalcycles) {
  (void)totalcycles;
  if (!g_bSpkrToggleFlag) {
    if (!g_nSpkrQuietCycleCount) {
      g_nSpkrQuietCycleCount = g_nCumulativeCycles;
    } else if (g_nCumulativeCycles - g_nSpkrQuietCycleCount > static_cast<uint64_t>(g_fCurrentCLK6502) / 5) {
      // After 0.2 sec of Apple time, deactivate spkr voice
      Spkr_SetActive(false);
    }
  } else {
    g_nSpkrQuietCycleCount = 0;
    g_bSpkrToggleFlag = false;
  }
  g_nSpkrLastCycle = g_nCumulativeCycles;
}

auto SpkrGetEvents(SpkrEvent *events, int max_events) -> int {
  int count = (g_nNumSpkrEvents < max_events) ? g_nNumSpkrEvents : max_events;
  if (count > 0) {
    memcpy(events, g_spkrEvents, count * sizeof(SpkrEvent));
    g_nNumSpkrEvents = 0;
  }
  return count;
}

auto SpkrGetLastCycle() -> uint64_t {
  return g_nSpkrLastCycle;
}

auto SpkrGetCurrentState() -> bool {
  return g_bSpkrState;
}

auto SpkrGetSnapshot(SS_IO_Speaker *pSS) -> uint32_t {
  pSS->g_nSpkrLastCycle = g_nSpkrLastCycle;
  return 0;
}

auto SpkrSetSnapshot(SS_IO_Speaker *pSS) -> uint32_t {
  g_nSpkrLastCycle = pSS->g_nSpkrLastCycle;
  return 0;
}
