#include "core/Common.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <SDL3/SDL.h>
#include "apple2/Disk.h"
#include "frontends/sdl3/DiskChoose.h"
#include "apple2/DiskFTP.h"
#include "apple2/ftpparse.h"
#include "frontends/sdl3/Frame.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "core/Util_Text.h"
#include "apple2/DiskCommands.h"

// Map error codes to human-readable strings
static const char* Disk_GetErrorMessage(int error_code) {
  switch (static_cast<DiskError_e>(error_code)) {
    case DISK_ERR_NONE:
      return "Success";
    case DISK_ERR_FILE_NOT_FOUND:
      return "Disk image file not found.";
    case DISK_ERR_IO:
      return "I/O error reading the disk image.";
    case DISK_ERR_UNSUPPORTED_FORMAT:
      return "Unsupported or unrecognized disk format.";
    case DISK_ERR_CORRUPT:
      return "The disk image appears to be corrupt or malformed.";
    case DISK_ERR_OUT_OF_MEMORY:
      return "System ran out of memory while loading the disk.";
    case DISK_ERR_WRITE_PROTECTED:
      return "The disk or file is write protected.";
    default:
      return "An unknown error occurred while loading the disk.";
  }
}

void DiskSelectImage(int drive, char* pszFilename) {
  (void)pszFilename;
  static size_t fileIndex = 0;
  static size_t backdx = 0;
  static size_t dirdx = 0;

  std::string filename;
  std::string fullPath;
  bool isdir = false;

  fileIndex = backdx;
  isdir = true;
  fullPath = g_state.sCurrentDir;

  while (isdir) {
    if (!ChooseAnImage(g_state.ScreenWidth, g_state.ScreenHeight, fullPath, 6,
                       filename, isdir, fileIndex)) {
      DrawFrameWindow();
      return;
    }
    if (isdir) {
      if (filename == "..")
      {
        const auto last_sep_pos = fullPath.find_last_of(FILE_SEPARATOR);
        if (last_sep_pos != std::string::npos) {
          fullPath = fullPath.substr(0, last_sep_pos);
        }
        if (fullPath == "") {
          fullPath = "/";
        }
        fileIndex = dirdx;

      } else {
        if (fullPath != "/") {
          fullPath += "/" + filename;
        } else {
          fullPath = "/" + filename;
        }
        dirdx = fileIndex;
        fileIndex = 0;
      }
    }
  }
  strcpy(g_state.sCurrentDir, fullPath.c_str());
  Configuration::Instance().SetString("Preferences", REGVALUE_PREF_START_DIR, g_state.sCurrentDir);
  Configuration::Instance().Save();

  fullPath += "/" + filename;

  int error = DiskInsert(drive, fullPath.c_str(), false, true);
  if (error == static_cast<int>(DISK_ERR_NONE)) {
    if (drive == 0) {
      Configuration::Instance().SetString("Preferences", REGVALUE_DISK_IMAGE1, fullPath.c_str());
    }
    else {
      Configuration::Instance().SetString("Preferences", REGVALUE_DISK_IMAGE2, fullPath.c_str());
    }
    Configuration::Instance().Save();
  } else {
    // Report specific error to user via Message Box
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Disk Loading Error", 
                             Disk_GetErrorMessage(error), nullptr);
  }
  backdx = fileIndex;  //store cursor position
  DrawFrameWindow();
}

void DiskSelect(int drive) {
  char szSelect[] = "";
  DiskSelectImage(drive, szSelect);  // drive is 0 for D1, 1 - for D2
}

void Disk_FTP_SelectImage(int drive) {
  // FTP selection logic...
  // For now, this is a placeholder/stub to be refined in later milestones.
  (void)drive;
}
