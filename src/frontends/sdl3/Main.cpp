#include <SDL3/SDL.h>
#include <getopt.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "apple2/Riff.h"
#include "apple2/SaveState.h"
#include "apple2/SoundCore.h"
#include "apple2/Video.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/LinAppleCore.h"
#include "core/Log.h"
#include "core/Peripheral_Internal.h"
#include "core/Registry.h"
#include "core/Util_Text.h"
#include "frontends/sdl3/Frame.h"
#include "frontends/sdl3/Frontend.h"

// SDL Audio Stream for Frontend
bool g_bDSAvailable = false;
SDL_AudioStream* g_audioStream = nullptr;
static char* g_pszAudioDumpFile = nullptr;

static void SDLCALL sdl3AudioCallback(void* userdata, SDL_AudioStream* stream,
                                      int additional_amount, int total_amount) {
  (void)userdata;
  (void)total_amount;
  if (additional_amount <= 0) return;

  auto* temp_buf =
      static_cast<int16_t*>(SDL_malloc(static_cast<size_t>(additional_amount)));
  if (!temp_buf) return;

  int num_samples = additional_amount / (static_cast<int>(sizeof(int16_t)));
  SoundCore_GetSamples(temp_buf, static_cast<size_t>(num_samples));

  if (g_pszAudioDumpFile) {
    RiffPutSamples(reinterpret_cast<int16_t*>(temp_buf),
                   static_cast<uint32_t>(num_samples));
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

  g_audioStream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, sdl3AudioCallback, nullptr);
  if (g_audioStream == nullptr) {
    printf("Unable to open SDL audio: %s\n", SDL_GetError());
    return false;
  }

  SDL_ResumeAudioStreamDevice(g_audioStream);
  g_bDSAvailable = true;

  Linapple_SetAudioCallback(
      [](const int16_t* samples, size_t num_samples) -> void {
        DSUploadBuffer(const_cast<int16_t*>(samples),
                       static_cast<unsigned>(num_samples));
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

extern void SDL_HandleEvent(SDL_Event* e);

void Sys_Input() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    SDL_HandleEvent(&event);
  }
}

void EnterMessageLoop() {
  while (g_state.mode != MODE_EXIT) {
    Sys_Input();

    Linapple_RunFrame(17030);
    DrawFrameWindow();
    SDL_Delay(16);
  }
}

static void PrintHelp() {
  printf("LinApple SDL3 Frontend\n");
  printf("Usage: linapple [options]\n");
  printf("Options:\n");
  printf("  -1, --d1 <file>      Insert disk image in drive 1\n");
  printf("  -2, --d2 <file>      Insert disk image in drive 2\n");
  printf("  -a, --autoboot       Boot the computer immediately\n");
  printf("  -b, --boot           Synonym for --autoboot\n");
  printf("  -c, --config <file>  Use specified configuration file\n");
  printf("  -f, --fullscreen     Start in fullscreen mode\n");
  printf("  -h, --help           Display this help message\n");
  printf("  -l, --log            Enable logging to console\n");
  printf("  -m, --benchmark      Run a video benchmark and exit\n");
  printf("  -p, --pal            Enable PAL video mode\n");
  printf("  -P, --program <file> Load APL/PRG program file\n");
  printf("  -s, --snapshot <f>   Load state from snapshot file\n");
  printf("  -v, --verbose        Enable verbose performance logging\n");
  printf("  -x, --script <file>  Execute debugger script on startup\n");
  printf("  -T, --test-cpu <f>   Run 6502 functional test from binary file\n");
  printf("  -X, --test-trap <n>  Expected trap address for test-cpu (hex)\n");
  printf("  -6, --test-6502      Set Apple2+ mode for testing\n");
  printf("  -C, --test-65c02     Set Enhanced //e mode for testing\n");
}

auto main(int argc, char* argv[]) -> int {
  const char* szImageName_drive1 = nullptr;
  const char* szImageName_drive2 = nullptr;
  const char* szProgramName = nullptr;
  const char* szSnapshotFile = nullptr;
  const char* szConfigurationFile = nullptr;
  const char* szTestFile = nullptr;
  uint16_t uTestTrap = 0x336D;  // Default for NMOS
  const char* szHardwareName = nullptr;
  bool bBoot = false;
  bool bBenchMark = false;
  bool bSetFullScreen = false;
  bool bLog = false;
  bool bPAL = false;
  bool bTestCpu = false;
  bool bListHardware = false;

  int opt = 0;
  int optind = 0;
  static const struct option OptionTable[] = {
      {"d1", required_argument, nullptr, '1'},
      {"d2", required_argument, nullptr, '2'},
      {"autoboot", no_argument, nullptr, 'a'},
      {"boot", no_argument, nullptr, 'b'},
      {"config", required_argument, nullptr, 'c'},
      {"fullscreen", no_argument, nullptr, 'f'},
      {"help", no_argument, nullptr, 'h'},
      {"log", no_argument, nullptr, 'l'},
      {"benchmark", no_argument, nullptr, 'm'},
      {"pal", no_argument, nullptr, 'p'},
      {"program", required_argument, nullptr, 'P'},
      {"snapshot", required_argument, nullptr, 's'},
      {"script", required_argument, nullptr, 'x'},
      {"test-cpu", required_argument, nullptr, 'T'},
      {"test-trap", required_argument, nullptr, 'X'},
      {"test-6502", no_argument, nullptr, '6'},
      {"test-65c02", no_argument, nullptr, 'C'},
      {"verbose", no_argument, nullptr, 'v'},
      {"audio-dump", required_argument, nullptr, 'A'},
      {"list-hardware", no_argument, nullptr, 0x100},
      {"hardware-info", required_argument, nullptr, 0x101},
      {nullptr, 0, nullptr, 0}};

  while ((opt = getopt_long(argc, argv, "1:2:abc:fhlmpP:r:s:vx:T:X:6CA:",
                            OptionTable, &optind)) != -1) {
    switch (opt) {
      case '1':
        szImageName_drive1 = optarg;
        break;
      case '2':
        szImageName_drive2 = optarg;
        break;
      case 'P':
        szProgramName = optarg;
        break;
      case 's':
        szSnapshotFile = optarg;
        break;
      case 'c':
        szConfigurationFile = optarg;
        break;
      case 'b':
        bBoot = true;
        break;
      case 'm':
        bBenchMark = true;
        break;
      case 'f':
        bSetFullScreen = true;
        break;
      case 'l':
        bLog = true;
        break;
      case 'p':
        bPAL = true;
        break;
      case 'v':
        Logger::SetVerbosity(LogLevel::kPerf);
        break;
      case 'x':
        Util_SafeStrCpy(g_state.sDebuggerScript, optarg, PATH_MAX_LEN);
        break;
      case 'T':
        bTestCpu = true;
        szTestFile = optarg;
        break;
      case 'X':
        uTestTrap = static_cast<uint16_t>(strtol(optarg, nullptr, 0));
        break;
      case '6':
        g_Apple2Type = A2TYPE_APPLE2PLUS;
        break;
      case 'C':
        g_Apple2Type = A2TYPE_APPLE2EENHANCED;
        break;
      case 'A':
        g_pszAudioDumpFile = SDL_strdup(optarg);
        break;
      case 'h':
        PrintHelp();
        return 0;
      case 0x100:
        bListHardware = true;
        break;
      case 0x101:
        szHardwareName = optarg;
        break;
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

  if (bTestCpu) {
    Linapple_CpuTest(szTestFile, uTestTrap);
    return 0;
  }

  if (SysInit(bLog) != 0) return 1;

  do {
    g_state.restart = false;
    if (SessionInit(szConfigurationFile, bSetFullScreen, szImageName_drive1,
                    szImageName_drive2, szProgramName, szSnapshotFile, bBoot,
                    bPAL) != 0) {
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
