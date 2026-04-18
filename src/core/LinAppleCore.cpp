#include "LinAppleCore.h"
#include "core/Common.h"
#include <cstdio>
#include <cinttypes>
#include <chrono>
#include <array>
#include "apple2/Keyboard.h"
#include "apple2/Speaker.h"
#include "apple2/Disk.h"
#include "apple2/DiskImage.h"
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

// Forward declarations for coupled frontend functions (to be decoupled in later phases)
extern void SSCFrontend_Update(struct SuperSerialCard*, uint32_t);
extern void PrinterFrontend_Update(uint32_t);
extern void UpdateDisplay(int);
// Non-const globals are required for the procedural core bridge architecture.
extern struct SuperSerialCard sg_SSC; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static LinappleVideoCallback g_videoCB = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static LinappleAudioCallback g_audioCB = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static LinappleTitleCallback g_titleCB = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static uint8_t g_nRepeatKey = 0;              // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static uint32_t g_nRepeatDelayCycles = 0;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_bRepeating = false;             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const uint32_t KEY_REPEAT_INITIAL_DELAY = 512000;
const uint32_t KEY_REPEAT_RATE = 68000;

const int JOYSTICK_AXIS_CENTER = 127;
const int JOYSTICK_AXIS_RANGE = 32768;
const int JOYSTICK_AXIS_MAX = 255;
const int JOYSTICK_AXIS_DIVISOR = 65535;

const int APPLE_MEM_SIZE = 65536;
const uint16_t CPU_TEST_START_PC = 0x0400;
const uint16_t CPU_TEST_TRAP_PC = 0x3469;
const uint64_t CPU_TEST_MAX_CYCLES = 100000000;

const int SPKR_BUFFER_SIZE = 8192;
const int16_t SPKR_SAMPLE_VOLUME = 0x4000;

const int FULL_SPEED_DISK_ITERATIONS = 100;

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

void Linapple_SetCapsLockState(bool bEnabled) {
  KeybSetCapsLock(bEnabled);
}

extern "C" void Linapple_SetAppleKey(int apple_key, bool bDown) {
  JoySetRawButton(apple_key, bDown);
}

// Axis and Value are ints to maintain compatibility with the public API and various frontends.
extern "C" void Linapple_SetJoystickAxis(int axis, int value) { // NOLINT(bugprone-easily-swappable-parameters)
    static int s_joyX = JOYSTICK_AXIS_CENTER;
    static int s_joyY = JOYSTICK_AXIS_CENTER;
    int joy_val = ((value + JOYSTICK_AXIS_RANGE) * JOYSTICK_AXIS_MAX) / JOYSTICK_AXIS_DIVISOR;
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

void Linapple_SetVideoCallback(LinappleVideoCallback cb) {
    g_videoCB = cb;
}

void Linapple_SetAudioCallback(LinappleAudioCallback cb) {
    g_audioCB = cb;
}

void Linapple_SetTitleCallback(LinappleTitleCallback cb) {
    g_titleCB = cb;
}

void Linapple_UpdateTitle(const char* title) {
    if (g_titleCB) {
        g_titleCB(title);
    }
}

#include "core/Peripheral_Internal.h"

void Linapple_Init() {
  // Load globals from configuration
  uint32_t apple2Type = 0;
  if (LOAD("Computer Emulation", &apple2Type)) {
    g_Apple2Type = static_cast<eApple2Type>(apple2Type);
  }
  LOAD("Harddisk Enable", &hddenabled);
  LOAD("Clock Enable", &clockslot);
  LOAD("Save State On Exit", &g_bSaveStateOnExit);

  std::string snapshotName;
  if (LOAD("Save State Filename", &snapshotName) && !snapshotName.empty()) {
    Snapshot_SetFilename(snapshotName.c_str());
  } else {
    Snapshot_SetFilename(""); // Use default
  }

  MemPreInitialize();
  Asset_Init();
  ImageInitialize();
  DiskInitialize();
  CreateColorMixMap();
  SoundCore_Initialize();

  MemInitialize();
  CpuInitialize();
  VideoInitialize();

  Peripheral_Manager_Init();
  Peripheral_Plugins_Init();
  Peripheral_Register_Internal();

  KeybReset();
  JoyReset();

  uint8_t* pCxRomPeripheral = MemGetAuxPtr(APPLE_SLOT_BEGIN);
  (void)pCxRomPeripheral;

#ifndef HEADLESS
  DebugInitialize();
#endif
}

void Linapple_Shutdown() {
  Peripheral_Manager_Shutdown();
  Peripheral_Plugins_Shutdown();
  DiskDestroy();
  VideoDestroy();
  MemDestroy();
  CpuDestroy();
  SoundCore_Destroy();
  Asset_Quit();
}

void Linapple_CpuTest(const char* szTestFile) {
  if (!szTestFile) return;

  Linapple_Init();

  FilePtr f(fopen(szTestFile, "rb"), fclose);
  if (!f) return;

  fread(mem, 1, APPLE_MEM_SIZE, f.get());

  regs.pc = CPU_TEST_START_PC;
  uint64_t count = 0;
  while (count < CPU_TEST_MAX_CYCLES) {
    uint32_t executed = CpuExecute(1);
    count += executed;
    if (regs.pc == CPU_TEST_TRAP_PC) {
      // C-style varargs are used by the project's established Logger utility.
      Logger::Info("CPU trapped at 0x%04X after %" PRIu64 " cycles\n", regs.pc, // NOLINT(cppcoreguidelines-pro-type-vararg)
                   count);
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
  bool spkr_active = false;
  bool mb_active = false;
#if defined(ENABLE_PERIPHERAL_SPEAKER)
  spkr_active = Spkr_IsActive();
#endif
#if defined(ENABLE_PERIPHERAL_MOCKINGBOARD)
  mb_active = MB_IsActive();
#endif

  bool shouldTurbo = DiskIsSpinning() && enhancedisk && !spkr_active && !mb_active;

  static bool s_wasTurbo = false;
  static uint32_t s_turboStartMs = 0;

  if (shouldTurbo && !s_wasTurbo) {
    s_turboStartMs = Linapple_GetTicks();
    // C-style varargs are used by the project's established Logger utility.
    Logger::Perf("Full-speed disk mode engaged\n"); // NOLINT(cppcoreguidelines-pro-type-vararg)
  } else if (!shouldTurbo && s_wasTurbo) {
    uint32_t elapsed = Linapple_GetTicks() - s_turboStartMs;
    // C-style varargs are used by the project's established Logger utility.
    Logger::Perf("Full-speed disk mode disengaged after %ums\n", elapsed); // NOLINT(cppcoreguidelines-pro-type-vararg)
  }

  s_wasTurbo = shouldTurbo;
  g_bFullSpeed = shouldTurbo;
  return shouldTurbo;
}

#if defined(ENABLE_PERIPHERAL_SPEAKER)
// Global audio buffer is required for efficient sample accumulation between frames.
static std::array<int16_t, SPKR_BUFFER_SIZE> g_spkrBuffer; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

void SpkrFrontend_Update(uint32_t dwExecutedCycles) {
  if (dwExecutedCycles == 0) return;

#if defined(ENABLE_PERIPHERAL_SPEAKER)
  static bool s_lastState = false;
  static double s_nextSampleCycle = 0;
  double clksPerSample = g_fCurrentCLK6502 / SPKR_SAMPLE_RATE;

  std::array<SpkrEvent, MAX_SPKR_EVENTS> events{};
  int num_events = SpkrGetEvents(events.data(), static_cast<int>(events.size()));
  int event_idx = 0;

  uint64_t startCycle = g_nCumulativeCycles - dwExecutedCycles;
  uint64_t endCycle = g_nCumulativeCycles;

  if (s_nextSampleCycle < static_cast<double>(startCycle)) {
    s_nextSampleCycle = static_cast<double>(startCycle);
  }

  int numSamples = 0;
  while (s_nextSampleCycle < static_cast<double>(endCycle) && numSamples < (SPKR_BUFFER_SIZE - 2)) {
    // Direct indexing is used here for performance in the hot emulation loop.
    while (event_idx < num_events && static_cast<double>(events[static_cast<size_t>(event_idx)].cycle) <= s_nextSampleCycle) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      s_lastState = events[static_cast<size_t>(event_idx)].state; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      event_idx++;
    }
    int16_t val = s_lastState ? SPKR_SAMPLE_VOLUME : -SPKR_SAMPLE_VOLUME;
    // Direct indexing is used here for performance in the hot emulation loop.
    g_spkrBuffer[static_cast<size_t>(numSamples++)] = val; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    g_spkrBuffer[static_cast<size_t>(numSamples++)] = val; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    s_nextSampleCycle += clksPerSample;
  }

  if (numSamples > 0) {
    if (g_audioCB) {
        g_audioCB(g_spkrBuffer.data(), static_cast<size_t>(numSamples));
    } else {
        DSUploadBuffer(g_spkrBuffer.data(), numSamples);
    }
  }
#else
  (void)dwExecutedCycles;
#endif
}

static auto Internal_RunCycles(uint32_t dwCycles) -> uint32_t {
  if (dwCycles == 0) return 0;

  uint32_t dwExecutedCycles = CpuExecute(dwCycles);
  cyclenum += dwExecutedCycles;
  cumulativecycles = g_nCumulativeCycles;

  Peripheral_Manager_Think(dwExecutedCycles);

  VideoUpdateVbl(dwExecutedCycles);
  JoyUpdatePosition(dwExecutedCycles);

  SpkrFrontend_Update(dwExecutedCycles);
  Linapple_KeyboardThink(dwExecutedCycles);

  return dwExecutedCycles;
}

auto Linapple_RunFrame(uint32_t cycles) -> uint32_t {
  if (g_state.mode == MODE_RUNNING) {
#ifndef HEADLESS
    if (IsDebugSteppingAtFullSpeed()) {
        DebugContinueStepping();
        return 0;
    }
#endif

    uint32_t executed = 0;
    if (ShouldRunFullSpeed()) {
      for (int i = 0; i < FULL_SPEED_DISK_ITERATIONS; i++) {
        executed += Internal_RunCycles(cycles);
        if (!DiskIsSpinning()) break;
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
  } else if (g_state.mode == MODE_STEPPING) {
    uint32_t dwExecutedCycles = CpuExecute(1);
    cyclenum += dwExecutedCycles;
    g_nCumulativeCycles += dwExecutedCycles;
    VideoUpdateVbl(dwExecutedCycles);
#ifndef HEADLESS
    UpdateDisplay(UPDATE_ALL);
#endif
    return dwExecutedCycles;
  }
  return 0;
}
