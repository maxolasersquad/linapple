/*
 * ProgramLoader.h - Centralised program image loading (APL/PRG)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum ProgramLoadResult_e {
  PROGRAM_LOAD_OK = 0,
  PROGRAM_LOAD_NOT_A_PROGRAM = 1, /* file is not APL or PRG format */
  PROGRAM_LOAD_FILE_ERROR = 2,    /* could not open or read file */
  PROGRAM_LOAD_INVALID = 3        /* header valid but data out of range */
};

/**
 * @brief Attempt to load the file as an APL or PRG image.
 *
 * Returns PROGRAM_LOAD_NOT_A_PROGRAM if the file is neither format,
 * allowing the caller to fall through to disk insertion.
 */
enum ProgramLoadResult_e ProgramLoader_TryLoad(const char* path);

#ifdef __cplusplus
}
#endif
