#pragma once

#include <cstring>
#include "core/Common.h"

enum AppIntent {
  INTENT_RUN,
  INTENT_DIAGNOSTIC,
  INTENT_HELP,
  INTENT_ERROR
};

struct AppConfig {
  AppIntent intent;
  char szDiskPath[2][PATH_MAX_LEN];
  char szProgramPath[PATH_MAX_LEN];
  char szConfigPath[PATH_MAX_LEN];
  char szSnapshotPath[PATH_MAX_LEN];
  char szAudioDumpPath[PATH_MAX_LEN];

  eApple2Type apple2Type;
  bool bPAL;
  bool bFullscreen;
  bool bBoot;
  bool bBenchmark;
  bool bLog;
  bool bVerbose;

  bool bListHardware;
  char szHardwareInfoName[PATH_MAX_LEN];

  // Test/Diagnostic fields
  char szTestCpuFile[PATH_MAX_LEN];
  uint16_t uTestCpuTrap;
  char szDebuggerScript[PATH_MAX_LEN];

  // Extra args for frontend pass-through
  int argc_extra;
  const char* argv_extra[64];
};

/**
 * Initialize AppConfig with default values.
 */
inline void AppConfig_Default(AppConfig* pConfig) {
  if (pConfig) {
    memset(pConfig, 0, sizeof(AppConfig));
    pConfig->intent = INTENT_RUN;
    pConfig->apple2Type = A2TYPE_APPLE2EENHANCED;
    pConfig->uTestCpuTrap = 0x336D; // Default for NMOS
  }
}
