/*
linapple : An Apple //e emulator for Linux

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

#include "core/LinAppleCore.h"
#include "core/ProgramLoader.h"
#include "core/Common.h"
#include <cstdio>
#include <cinttypes>
#include <chrono>
#include <array>
#include <vector>
#include <cstring>
#include "apple2/Keyboard.h"
#include "apple2/Speaker.h"
#include "apple2/Mockingboard.h"
#include "apple2/SoundCore.h"
#include "apple2/Video.h"
#include "apple2/Memory.h"
#include "apple2/CPU.h"
#include "apple2/Clock.h"
#include "apple2/SerialComms.h"
#include "apple2/Joystick.h"
#include "apple2/SaveState.h"
#ifndef HEADLESS
#include "Debugger/Debug.h"
#endif
#include "core/Common_Globals.h"
#include "core/Log.h"
#include "apple2/ParallelPrinter.h"
#include "core/asset.h"
#include "core/Peripheral.h"
#include "core/Peripheral_Internal.h"

using Logger::Error;
using Logger::Info;

// Constants from original implementation
const int APPLE_MEM_SIZE = 65536;
const uint16_t CPU_TEST_START_PC = 0x0800;
const uint16_t CPU_TEST_TRAP_PC = 0x3469; // Example trap PC
const uint64_t CPU_TEST_MAX_CYCLES = 100000000;
const int FULL_SPEED_DISK_ITERATIONS = 100;

// Repetition logic
static uint8_t g_nRepeatKey = 0;
static uint32_t g_nRepeatDelayCycles = 0;
static bool g_bRepeating = false;
const uint32_t KEY_REPEAT_INITIAL_DELAY = 512000;
const uint32_t KEY_REPEAT_RATE = 68000;

// Callbacks
static LinappleVideoCallback g_videoCB = nullptr;
static LinappleAudioCallback g_audioCB = nullptr;
static LinappleTitleCallback g_titleCB = nullptr;

static uint32_t s_turboStartMs = 0;
static bool s_wasTurbo = false;

void Linapple_SetVideoCallback(LinappleVideoCallback cb) { g_videoCB = cb; }
void Linapple_SetAudioCallback(LinappleAudioCallback cb) { g_audioCB = cb; }
void Linapple_SetTitleCallback(LinappleTitleCallback cb) { g_titleCB = cb; }

void Linapple_UpdateTitle(const char* title) {
  if (g_titleCB) {
    g_titleCB(title);
  }
}

void Linapple_KeyboardThink(uint32_t dwCycles) {
  if (g_nRepeatKey == 0) return;
  g_nRepeatDelayCycles += dwCycles;
  if (!g_bRepeating) {
    if (g_nRepeatDelayCycles >= KEY_REPEAT_INITIAL_DELAY) {
      g_bRepeating = true;
      g_nRepeatDelayCycles = 0;
      KeybPushAppleKey(g_nRepeatKey);
    }
  } else {
    if (g_nRepeatDelayCycles >= KEY_REPEAT_RATE) {
      g_nRepeatDelayCycles = 0;
      KeybPushAppleKey(g_nRepeatKey);
    }
  }
}

extern "C" void Linapple_SetKeyState(uint8_t apple_code, bool bDown) {
  if (bDown) {
    if (g_nRepeatKey == apple_code) return;
    g_nRepeatKey = apple_code;
    g_nRepeatDelayCycles = 0;
    g_bRepeating = false;
    if (apple_code) KeybPushAppleKey(apple_code);
  } else {
    if (g_nRepeatKey == apple_code) {
      g_nRepeatKey = 0;
      g_bRepeating = false;
    }
  }
}

extern "C" void Linapple_SetCapsLockState(bool bEnabled) {
  KeybSetCapsLock(bEnabled);
}

extern "C" void Linapple_SetAppleKey(int apple_key, bool bDown) {
  JoySetRawButton(apple_key, bDown);
}

extern "C" void Linapple_SetJoystickAxis(int axis, int value) {
  static int s_joyX = 127;
  static int s_joyY = 127;
  int joy_val = ((value + 32768) * 255) / 65535;
  if (axis == 0) {
    s_joyX = joy_val;
  } else if (axis == 1) {
    s_joyY = joy_val;
  }
  JoySetRawPosition(0, s_joyX, s_joyY);
}

extern "C" void Linapple_SetJoystickButton(int button, bool down) {
  JoySetRawButton(button, down);
}

void Linapple_Init() {
  MemPreInitialize();
  Asset_Init();
  CreateColorMixMap();
  SoundCore_Initialize();

  MemInitialize();
  CpuInitialize();
  VideoInitialize();

  g_state.mode = MODE_RUNNING;

  Peripheral_Manager_Init();
  Peripheral_Plugins_Init();
  Peripheral_Register_Internal();

  KeybReset();
  JoyReset();
}

void Linapple_Shutdown() {
  Peripheral_Manager_Shutdown();
  Peripheral_Plugins_Shutdown();
  VideoDestroy();
  MemDestroy();
  CpuDestroy();
  SoundCore_Destroy();
  Asset_Quit();
}

int Linapple_LoadProgram(const char* path) {
  return static_cast<int>(ProgramLoader_TryLoad(path));
}

static auto Internal_RunCycles(uint32_t dwCycles) -> uint32_t;

void Linapple_CpuTest(const char* szTestFile, uint16_t trap_addr) {
  if (!szTestFile) return;

  Linapple_Init();
  g_state.mode = MODE_RUNNING;

  FilePtr f(fopen(szTestFile, "rb"), fclose);
  if (!f) return;

  fread(mem, 1, APPLE_MEM_SIZE, f.get());

  regs.pc = 0x0400; // NMOS 6502 functional test entry
  uint64_t count = 0;
  while (count < CPU_TEST_MAX_CYCLES) {
    uint32_t executed = CpuExecute(1);
    cyclenum += executed;
    g_nCumulativeCycles += executed;
    count += executed;
    if (regs.pc == trap_addr) {
      printf("CPU trapped at 0x%04X after %" PRIu64 " cycles\n", regs.pc, count);
      break;
    }
  }
  Linapple_Shutdown();
}

auto Linapple_GetTicks() -> uint32_t {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
}

static auto ShouldRunFullSpeed() -> bool {
  bool mb_active = false;
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
  mb_active = MB_IsActive();
#endif

  bool spkr_active = Spkr_IsActive();
  bool peripheral_active = Peripheral_IsAnyActive();

  bool shouldTurbo =
      peripheral_active && (g_state.needsprecision == 0) && !mb_active && !spkr_active;

  if (shouldTurbo && !s_wasTurbo) {
    s_turboStartMs = Linapple_GetTicks();
    Logger::Perf("Full-speed disk mode engaged\n");
  } else if (!shouldTurbo && s_wasTurbo) {
    uint32_t elapsed = Linapple_GetTicks() - s_turboStartMs;
    Logger::Perf("Full-speed disk mode disengaged after %ums\n", elapsed);
  }

  s_wasTurbo = shouldTurbo;
  g_bFullSpeed = shouldTurbo;
  return shouldTurbo;
}

static auto Internal_RunCycles(uint32_t dwCycles) -> uint32_t {
  if (dwCycles == 0) return 0;

  uint32_t dwExecutedCycles = CpuExecute(dwCycles);
  cyclenum += dwExecutedCycles;
  g_nCumulativeCycles += dwExecutedCycles;

  Peripheral_Manager_Think(dwExecutedCycles);

  VideoUpdateVbl(dwExecutedCycles);
  JoyUpdatePosition(dwExecutedCycles);

  SpkrUpdate(dwExecutedCycles);
  Linapple_KeyboardThink(dwExecutedCycles);

  return dwExecutedCycles;
}

auto Linapple_RunFrame(uint32_t cycles) -> uint32_t {
  if (g_state.mode == MODE_RUNNING) {
    uint32_t executed = 0;
    if (ShouldRunFullSpeed()) {
      for (int i = 0; i < FULL_SPEED_DISK_ITERATIONS; i++) {
        executed += Internal_RunCycles(cycles);
        if (!Peripheral_IsAnyActive()) break;
      }
    } else {
      executed = Internal_RunCycles(cycles);
    }

#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
    MB_EndOfVideoFrame();
#endif

    if (g_videoCB && g_bFrameReady) {
      uint32_t* output = VideoGetOutputBuffer();
      g_videoCB(output, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * 4);
      g_bFrameReady = false;
    }
    return executed;
  }
  return 0;
}
