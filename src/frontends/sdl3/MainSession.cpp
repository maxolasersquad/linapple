#include "frontends/sdl3/Frontend.h"

#include <SDL3/SDL.h>
#include <curl/curl.h>
#include <cstdio>
#include <cinttypes>
#include <string>

#include "core/Common.h"
#include "core/Common_Globals.h"
#include "frontends/sdl3/Frame.h"
#include "core/Log.h"
#include "core/Registry.h"
#include "apple2/CPU.h"
#include "apple2/Memory.h"
#include "apple2/Video.h"
#include "apple2/Disk.h"
#include "apple2/SaveState.h"
#include "core/LinAppleCore.h"
#include "core/Util_Path.h"

using Logger::Error;
using Logger::Info;

static bool g_bBudgetVideo = false;

void SetBudgetVideo(bool b) { g_bBudgetVideo = b; }
auto GetBudgetVideo() -> bool { return g_bBudgetVideo; }

void SetCurrentCLK6502() {
  g_fCurrentCLK6502 = 1.023 * 1000000.0;
}

void SoundCore_SetFade(int fade) { (void)fade; }

void SingleStep(bool bReinit) {
  (void)bReinit;
  Linapple_RunFrame(1);
}

auto SysInit(bool bLog) -> int {
  (void)bLog; // Logging is always enabled to XDG data dir

  Logger::Initialize();

  if (InitSDL() != 0) {
    return 1;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);
  g_curl = curl_easy_init();
  if (!g_curl) {
    Error("Could not initialize CURL easy interface\n");
    return 1;
  }
  curl_easy_setopt(g_curl, CURLOPT_USERPWD, g_state.sFTPUserPass);

  Linapple_Init();

  return 0;
}

void SysShutdown() {
  Linapple_Shutdown();

  DSShutdown();

  SDL_Quit();
  if (g_curl) {
    curl_easy_cleanup(g_curl);
    curl_global_cleanup();
  }
  Logger::Destroy();
}

static void Frontend_SetWindowTitle(const char* title) {
  extern SDL_Window* g_window;
  if (g_window) {
    SDL_SetWindowTitle(g_window, title);
  }
}

auto SessionInit(const char* szConfigurationFile, bool bSetFullScreen,
                const char* szImageName_drive1, const char* szImageName_drive2,
                const char* szSnapshotFile, bool bBoot, bool bPAL) -> int {
  if (szConfigurationFile) {
    Configuration::Instance().Load(szConfigurationFile);
  } else {
    std::string configPath = Path::FindDataFile("linapple.conf");
    if (!configPath.empty()) {
        Configuration::Instance().Load(configPath);
    } else {
        // Fallback if not found anywhere, config will save here later
        std::string fallbackPath = Path::GetUserConfigDir();
        Path::EnsureDirExists(fallbackPath);
        Configuration::Instance().Load(fallbackPath + "linapple.conf");
    }
  }

  Linapple_SetTitleCallback(Frontend_SetWindowTitle);

  if (bSetFullScreen) g_state.fullscreen = true;

  if (FrameCreateWindow() != 0) return 1;

  if (bPAL) g_videotype = VT_COLOR_TVEMU;

  if (szSnapshotFile) {
    Snapshot_SetFilename(szSnapshotFile);
  }
  Snapshot_Startup();

  if (szImageName_drive1) DiskInsert(DRIVE_1, szImageName_drive1, false, false);
  if (szImageName_drive2) DiskInsert(DRIVE_2, szImageName_drive2, false, false);

  if (bBoot) DiskBoot();

  DSInit();
  return 0;
}

void SessionShutdown() {
    // Session-specific cleanup if needed beyond core shutdown
}

void CpuTestHeadless(const char* szTestFile) {
  if (!szTestFile) return;

  Linapple_Init();

  FilePtr f(fopen(szTestFile, "rb"), fclose);
  if (!f) {
    return;
}

  fread(mem, 1, 65536, f.get());

  regs.pc = 0x0400;
  uint64_t count = 0;
  while (count < 100000000) {
    uint32_t executed = CpuExecute(1);
    count += executed;
    if (regs.pc == 0x3469) {
      Info("CPU trapped at 0x%04X after %" PRIu64 " cycles\n", regs.pc, count);
      break;
    }
  }
}
