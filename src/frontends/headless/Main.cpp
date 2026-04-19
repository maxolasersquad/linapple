#include <getopt.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "apple2/SaveState.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/LinAppleCore.h"
#include "core/Log.h"
#include "core/Peripheral_Internal.h"
#include "core/ProgramLoader.h"
#include "core/Registry.h"
#include "core/Util_Path.h"

void VideoCallback(const uint32_t* pixels, int width, int height, int pitch) {
  (void)pixels;
  (void)width;
  (void)height;
  (void)pitch;
}

void AudioCallback(const int16_t* samples, size_t num_samples) {
  (void)samples;
  (void)num_samples;
}

void TitleCallback(const char* title) { (void)title; }

auto main(int argc, char* argv[]) -> int {
  static struct option long_options[] = {
      {"test-cpu", required_argument, nullptr, 't'},
      {"test-trap", required_argument, nullptr, 'X'},
      {"test-6502", no_argument, nullptr, '6'},
      {"test-65c02", no_argument, nullptr, 'C'},
      {"boot", no_argument, nullptr, 'b'},
      {"d1", required_argument, nullptr, '1'},
      {"d2", required_argument, nullptr, '2'},
      {"program", required_argument, nullptr, 'P'},
      {"pal", no_argument, nullptr, 'p'},
      {"config", required_argument, nullptr, 'c'},
      {"list-hardware", no_argument, nullptr, 0x100},
      {"hardware-info", required_argument, nullptr, 0x101},
      {nullptr, 0, 0, 0}};

  const char* disk1 = nullptr;
  const char* disk2 = nullptr;
  const char* program = nullptr;
  const char* hardwareName = nullptr;
  const char* szConfigurationFile = nullptr;
  const char* cpuTestFile = nullptr;
  uint16_t cpuTestTrap = 0x336D;  // Default for NMOS
  bool listHardware = false;
  bool bPAL = false;

  int c = 0;
  int option_index = 0;
  while ((c = getopt_long(argc, argv, "t:b6C1:2:P:pc:X:", long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'c':
        szConfigurationFile = optarg;
        break;
      case 't':
        cpuTestFile = optarg;
        break;
      case 'X':
        cpuTestTrap = static_cast<uint16_t>(strtol(optarg, nullptr, 0));
        break;
      case '6':
        g_Apple2Type = A2TYPE_APPLE2PLUS;
        break;
      case 'C':
        g_Apple2Type = A2TYPE_APPLE2EENHANCED;
        break;
      case 'b':
        break;
      case '1':
        disk1 = optarg;
        break;
      case '2':
        disk2 = optarg;
        break;
      case 'P':
        program = optarg;
        break;
      case 'p':
        bPAL = true;
        break;
      case 0x100:
        listHardware = true;
        break;
      case 0x101:
        hardwareName = optarg;
        break;
    }
  }

  if (cpuTestFile) {
    Linapple_CpuTest(cpuTestFile, cpuTestTrap);
    return 0;
  }

  if (listHardware) {
    Linapple_ListHardware();
    return 0;
  }

  if (hardwareName) {
    Peripheral_t* p = Peripheral_Find_Internal(hardwareName);
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
      const char* path = Peripheral_GetPluginPath(hardwareName);
      if (path) {
        printf("Plugin Path: %s\n", path);
      }
    } else {
      fprintf(stderr, "Error: Unknown hardware '%s'\n", hardwareName);
      return 1;
    }
    return 0;
  }

  if (szConfigurationFile) {
    Configuration::Instance().Load(szConfigurationFile);
  } else {
    std::string configPath = Path::FindDataFile("linapple.conf");
    if (!configPath.empty()) {
      Configuration::Instance().Load(configPath);
    } else {
      std::string fallbackPath = Path::GetUserConfigDir();
      Path::EnsureDirExists(fallbackPath);
      Configuration::Instance().Load(fallbackPath + "linapple.conf");
    }
  }

  std::cout << "Starting LinApple Headless Frontend…" << std::endl;

  Logger::Initialize();
  Linapple_Init();
  Snapshot_Startup();
  Linapple_SetVideoCallback(VideoCallback);
  Linapple_SetAudioCallback(AudioCallback);
  Linapple_SetTitleCallback(TitleCallback);

  if (bPAL) {
    Configuration::Instance().SetInt("Configuration", "Video Emulation",
                                     1);  // VT_PAL
  }

  if (disk1) {
    Configuration::Instance().SetString("Slots", REGVALUE_DISK_IMAGE1, disk1);
  }
  if (disk2) {
    Configuration::Instance().SetString("Slots", REGVALUE_DISK_IMAGE2, disk2);
  }

  Linapple_RegisterPeripherals();

  if (program) {
    if (Linapple_LoadProgram(program) != 0) {
      fprintf(stderr, "Error: Could not load program '%s'\n", program);
      Linapple_Shutdown();
      return 1;
    }
  }

  g_state.mode = MODE_RUNNING;

  for (int i = 0; i < 60; ++i) {
    Linapple_RunFrame(17030);
  }

  Snapshot_Shutdown();
  Linapple_Shutdown();

  std::cout << "Headless execution complete." << std::endl;

  return 0;
}
