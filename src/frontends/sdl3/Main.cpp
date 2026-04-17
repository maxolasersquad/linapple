#include "core/Common.h"
#include <SDL3/SDL.h>
#include <getopt.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "frontends/sdl3/Frontend.h"
#include "apple2/SoundCore.h"
#include "apple2/Video.h"
#include "frontends/sdl3/Frame.h"
#include "core/Log.h"
#include "core/Common_Globals.h"
#include "core/Util_Text.h"
#include "Debugger/Debug.h"
#include "frontends/sdl3/DiskChoose.h"
#include "core/LinAppleCore.h"
#include "core/Peripheral_Internal.h"

#include "apple2/Riff.h"

// SDL Audio Stream for Frontend
// Justification: Global audio state managed by the frontend.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
bool g_bDSAvailable = false;
SDL_AudioStream *g_audioStream = nullptr;
static char* g_pszAudioDumpFile = nullptr;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): SDL3 callback signature
static void SDLCALL sdl3AudioCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
  (void)userdata;
  (void)total_amount;
  if (additional_amount <= 0) return;

  // Justification: SDL3 managed memory for audio callback.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  auto *temp_buf = static_cast<int16_t *>(SDL_malloc(static_cast<size_t>(additional_amount)));
  if (!temp_buf) return;

  int num_samples = additional_amount / (static_cast<int>(sizeof(int16_t))); // total shorts
  SoundCore_GetSamples(temp_buf, static_cast<size_t>(num_samples));

  if (g_pszAudioDumpFile) {
    // Justification: RiffPutSamples uses legacy short* pointer.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    RiffPutSamples(reinterpret_cast<int16_t*>(temp_buf), static_cast<uint32_t>(num_samples));
  }

  SDL_PutAudioStreamData(stream, temp_buf, additional_amount);
  SDL_free(temp_buf);
}

auto DSInit() -> bool {
  if (g_audioStream) return true;

  SDL_AudioSpec desired;
  desired.freq = SPKR_SAMPLE_RATE;
  desired.channels = 2;
  desired.format = SDL_AUDIO_S16;

  if (g_pszAudioDumpFile) {
    RiffInitWriteFile(g_pszAudioDumpFile, SPKR_SAMPLE_RATE, 2);
  }

  g_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, sdl3AudioCallback, nullptr);
  if (g_audioStream == nullptr) {
    // Justification: SDL initialization error reporting.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    printf("Unable to open SDL audio: %s\n", SDL_GetError());
    return false;
  }

  SDL_ResumeAudioStreamDevice(g_audioStream);
  g_bDSAvailable = true;

  // Register frontend callback to core
  Linapple_SetAudioCallback([](const int16_t* samples, size_t num_samples) -> void {
      // Justification: DSUploadBuffer uses legacy short* pointer.
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
      DSUploadBuffer(const_cast<int16_t*>(samples), static_cast<unsigned>(num_samples));
  });

  return true;
}

void DSShutdown() {
  if (g_pszAudioDumpFile) {
    RiffFinishWriteFile();
  }

  if (g_audioStream) {
    SDL_DestroyAudioStream(g_audioStream);
    g_audioStream = nullptr;
  }
}

extern void SDL_HandleEvent(SDL_Event *e);

void Sys_Input() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (g_state.mode == MODE_DISK_CHOOSE) {
      DiskChoose_Tick(&event);
      continue;
    }
    SDL_HandleEvent(&event);
  }
}

void Sys_Think(uint32_t dwCycles) {
  Linapple_RunFrame(dwCycles);
  if (g_state.mode == MODE_LOGO) {
    DrawAppleContent();
  } else if (g_state.mode == MODE_DEBUG) {
    DebuggerUpdate();
  }
}

void Sys_Draw() {
  if (g_state.mode == MODE_DISK_CHOOSE) {
    DiskChoose_Draw();
  } else {
    DrawFrameWindow();
  }
}

void EnterMessageLoop() {
  const int TICKS_PER_SECOND = 60;
  const int SKIP_TICKS = 1000 / TICKS_PER_SECOND;
  const int MAX_FRAMESKIP = 5;
  const uint32_t CYCLES_PER_TICK = 17030; // ~1.023 MHz / 60

  uint64_t next_game_tick = SDL_GetTicks();

  while (g_state.mode != MODE_EXIT) {
    int loops = 0;
    while (SDL_GetTicks() >= next_game_tick && loops < MAX_FRAMESKIP) {
      Sys_Input();
      Sys_Think(CYCLES_PER_TICK);
      next_game_tick += SKIP_TICKS;
      loops++;
    }

    uint64_t now = SDL_GetTicks();
    if (now < next_game_tick) {
        SDL_Delay(static_cast<uint32_t>(next_game_tick - now));
    } else {
        // If we are way behind, reset next_game_tick to avoid massive catch-up loop
        if (now > next_game_tick + 1000) {
            next_game_tick = now;
        }
    }

    Sys_Draw();
  }
}

static void PrintHelp() {
  printf("Linapple - Apple ][ emulator\n");
  printf("Usage: linapple [options]\n");
  printf("Options:\n");
  printf("  -1, --d1 <file>      Load disk image in drive 1\n");
  printf("  -2, --d2 <file>      Load disk image in drive 2\n");
  printf("  -b, --boot           Boot disk in drive 1\n");
  printf("  -f, --fullscreen     Start in fullscreen mode\n");
  printf("  -p, --pal            Use PAL video timing (default NTSC)\n");
  printf("  -v, --verbose        Enable verbose logging\n");
  printf("  --list-hardware      List all built-in peripheral hardware\n");
  printf("  --hardware-info <name> Show detailed information for a peripheral\n");
  printf("  -h, --help           Display this help\n");
}

auto main(int argc, char* argv[]) -> int {
  int opt = 0;
  const char* szImageName_drive1 = nullptr;
  const char* szImageName_drive2 = nullptr;
  const char* szSnapshotFile = nullptr;
  const char* szConfigurationFile = nullptr;
  const char* szTestFile = nullptr;
  const char* szHardwareName = nullptr;
  bool bBoot = false;
  bool bBenchMark = false;
  bool bSetFullScreen = false;
  bool bLog = false;
  bool bPAL = false;
  bool bTestCpu = false;
  bool bListHardware = false;

  // Justification: Table of command line options for getopt_long.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)
  static struct option OptionTable[] = {
    {"d1",           required_argument, nullptr, '1'},
    {"d2",           required_argument, nullptr, '2'},
    {"debug-script", required_argument, nullptr, 'x'},
    {"help",         no_argument,       nullptr, 'h'},
    {"pal",          no_argument,       nullptr, 'p'},
    {"state",        required_argument, nullptr, 's'},
    {"test-cpu",     required_argument, nullptr, 'T'},
    {"test-6502",    no_argument,       nullptr, '6'},
    {"test-65c02",   no_argument,       nullptr, 'C'},
    {"verbose",      no_argument,       nullptr, 'v'},
    {"audio-dump",   required_argument, nullptr, 'A'},
    {"list-hardware", no_argument,      nullptr, 0x100},
    {"hardware-info", required_argument, nullptr, 0x101},
    {nullptr, 0, nullptr, 0}
  };

  // Justification: Parsing command line arguments.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  while ((opt = getopt_long(argc, argv, "1:2:abc:fhlmpr:s:vx:T:6CA:", OptionTable, &optind)) != -1) {
    switch (opt) {
      case '1': szImageName_drive1 = optarg; break;
      case '2': szImageName_drive2 = optarg; break;
      case 's': szSnapshotFile = optarg; break;
      case 'c': szConfigurationFile = optarg; break;
      case 'b': bBoot = true; break;
      case 'm': bBenchMark = true; break;
      case 'f': bSetFullScreen = true; break;
      case 'l': bLog = true; break;
      case 'p': bPAL = true; break;
      case 'v': Logger::SetVerbosity(LogLevel::Perf); break;
      case 'x': 
        // Justification: Copying debugger script path.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        Util_SafeStrCpy(g_state.sDebuggerScript, optarg, MAX_PATH); 
        break;
      case 'T': bTestCpu = true; szTestFile = optarg; break;
      case '6': g_Apple2Type = A2TYPE_APPLE2PLUS; break;
      case 'C': g_Apple2Type = A2TYPE_APPLE2EENHANCED; break;
      case 'A': g_pszAudioDumpFile = SDL_strdup(optarg); break;
      case 'h': PrintHelp(); return 0;
      case 0x100: bListHardware = true; break;
      case 0x101: szHardwareName = optarg; break;
      default:
        fprintf(stderr, "Check --help for proper usage.\n");
        return 255;
    }
  }

  if (bListHardware) {
    Linapple_ListHardware();
    return 0;
  }

  if (szHardwareName) {
    Peripheral_t* p = Peripheral_Find_Internal(szHardwareName);
    if (p) {
        printf("Hardware Info: %s\n", p->name);
        printf("ABI Version: %d\n", p->abi_version);
        printf("Compatible Slots: ");
        bool first = true;
        for (int i = 0; i < NUM_SLOTS; ++i) {
            if (p->compatible_slots & (1u << static_cast<uint32_t>(i))) {
                if (!first) printf(", ");
                printf("%d", i);
                first = false;
            }
        }
        printf("\n");
        const char* path = Peripheral_GetPluginPath(szHardwareName);
        if (path) {
            printf("Plugin Path: %s\n", path);
        }
    } else {
        fprintf(stderr, "Error: Unknown hardware '%s'\n", szHardwareName);
        return 1;
    }
    return 0;
  }

  if (SysInit(bLog) != 0) return 1;

  if (bBoot) {
    g_state.mode = MODE_RUNNING;
  }

  if (bTestCpu) {
    CpuTestHeadless(szTestFile);
    SysShutdown();
    return 0;
  }

  // Justification: Main emulator message loop.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  do {
    g_state.restart = false;
    if (SessionInit(szConfigurationFile, bSetFullScreen,
                    szImageName_drive1, szImageName_drive2,
                    szSnapshotFile, bBoot, bPAL) != 0) {
      break;
    }

    if (bBoot) {
        VideoRedrawScreen();
    }

    if (bBenchMark) {
      VideoBenchmark();
    } else {
      EnterMessageLoop();
    }

    SessionShutdown();
  } while (g_state.restart);

  SysShutdown();
  return 0;
}
