/*
 * DiskCommands.h - LinApple Disk Peripheral Command Interface
 *
 * Command payloads sent through the queue must fit in a 512-byte slot.
 * DiskStatus_t exceeds this limit and is retrieved synchronously via
 * DISK_CMD_GET_STATUS instead.
 */

#pragma once

// NOLINTBEGIN(modernize-deprecated-headers,modernize-use-using,cppcoreguidelines-pro-type-member-init)
// Rationale: intentional C99 ABI header. <cstdint> and friends, 'using', and
// struct constructors are C++ only and cannot appear in a C99-compatible interface.

#include <stdint.h>
#include <stdbool.h>
// DiskError_e (used in DiskStatus_t) is defined here; extract to DiskError.h if
// the command layer ever needs to decouple from the full driver ABI.
#include "apple2/DiskFormatDriver.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { DISK_DEFAULT_SLOT = 6 };

typedef enum {
  DISK_DRIVE_0 = 0,
  DISK_DRIVE_1 = 1
} DiskDrive_e;

typedef enum {
  DISK_CMD_INSERT      = 0x0001,  /* payload: DiskInsertCmd_t */
  DISK_CMD_EJECT       = 0x0002,  /* payload: DiskEjectCmd_t */
  DISK_CMD_SWAP_DRIVES = 0x0003,  /* no payload */
  DISK_CMD_SET_PROTECT = 0x0004,  /* payload: DiskSetProtectCmd_t */
  DISK_CMD_GET_STATUS  = 0x0005   /* synchronous only; payload: DiskStatus_t* */
} DiskCmd_e;

/*
 * DISK_INSERT_PATH_MAX: DiskInsertCmd_t must fit in one 512-byte queue slot.
 * Field layout: 4 (DiskDrive_e) + 504 (path) + 1 (bool) + 1 (bool) = 510,
 * plus 2 bytes of implicit tail padding for 4-byte struct alignment = 512.
 */
enum {
  DISK_INSERT_PATH_MAX = 504,
  DISK_STATUS_NAME_MAX =  64,  /* filename without path */
  DISK_STATUS_PATH_MAX = 512   /* absolute path; silently truncated if longer */
};

/* sizeof(DiskInsertCmd_t) == 512; enforced by static_assert in the implementation */
typedef struct {
  DiskDrive_e drive;
  char        path[DISK_INSERT_PATH_MAX];  /* absolute path, null-terminated */
  bool        write_protected;
  bool        create_if_necessary;
} DiskInsertCmd_t;

typedef struct {
  DiskDrive_e drive;
} DiskEjectCmd_t;

typedef struct {
  DiskDrive_e drive;
  bool        write_protected;
} DiskSetProtectCmd_t;

/*
 * Too large for a queue slot (~1168 bytes). Used synchronously: caller passes
 * a pointer and the peripheral fills it before returning.
 * All char fields are null-terminated; paths are silently truncated to fit.
 */
typedef struct {
  bool        drive0_loaded;
  bool        drive0_spinning;
  bool        drive0_writing;
  bool        drive0_write_protected;
  DiskError_e drive0_last_error;
  char        drive0_name[DISK_STATUS_NAME_MAX];
  char        drive0_full_path[DISK_STATUS_PATH_MAX];

  bool        drive1_loaded;
  bool        drive1_spinning;
  bool        drive1_writing;
  bool        drive1_write_protected;
  DiskError_e drive1_last_error;
  char        drive1_name[DISK_STATUS_NAME_MAX];
  char        drive1_full_path[DISK_STATUS_PATH_MAX];
} DiskStatus_t;

// NOLINTEND(modernize-deprecated-headers,modernize-use-using,cppcoreguidelines-pro-type-member-init)

#ifdef __cplusplus
}
#endif
