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

  eApple2Type apple2Type;
  bool bPAL;
  bool bFullscreen;

  bool bListHardware;
  char szHardwareInfoName[PATH_MAX_LEN];
};

/**
 * Initialize AppConfig with default values.
 */
inline void AppConfig_Default(AppConfig* pConfig) {
  if (pConfig) {
    memset(pConfig, 0, sizeof(AppConfig));
    pConfig->intent = INTENT_RUN;
    pConfig->apple2Type = A2TYPE_APPLE2EENHANCED;
    pConfig->bPAL = false;
    pConfig->bFullscreen = false;
    pConfig->bListHardware = false;
  }
}
