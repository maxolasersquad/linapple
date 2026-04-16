#include <cstdint>
#pragma once

typedef struct tagSS_CARD_DISK2 SS_CARD_DISK2;

constexpr int DRIVE_1 = 0;
constexpr int DRIVE_2 = 1;

constexpr int DRIVES = 2;
constexpr int TRACKS = 35;
constexpr int SECTORS_PER_TRACK_16 = 16;
constexpr int SECTORS_PER_TRACK_13 = 13;
constexpr int GCR_ENCODE_TABLE_SIZE = 64;
constexpr int GCR_DECODE_TABLE_SIZE = 128;
constexpr int NUM_INTERLEAVE_MODES = 3;

constexpr int GCR_SECTOR_DATA_SIZE = 342;
constexpr int GCR_SECTOR_WITH_CHECKSUM_SIZE = 343;

constexpr int GAP3_SIZE = 27;
constexpr int DISK_SIZE_140K = 143360;
constexpr int MIN_140K_DISK_SIZE = 143105;
constexpr int MAX_140K_DISK_SIZE = 143364;
constexpr int DISK_SIZE_140K_ALT1 = 143403;
constexpr int DISK_SIZE_140K_ALT2 = 143488;
constexpr int MAX_PRODOS_VOLUME_SIZE = 32 * 1024 * 1024;
constexpr int PRODOS_BLOCK_SIZE = 512;
constexpr int DISK_SIZE_NIB = 232960; // 35 * 6656
constexpr int NIBBLES_PER_TRACK_NB2 = 6384;
constexpr int DISK_SIZE_NB2 = 223440; // 35 * 6384
constexpr int DOS_TRACK_SIZE = 4096;
constexpr int VTOC_TRACK = 17;
constexpr int VTOC_OFFSET = 0x11000;

const uint16_t GCR_WORK_BUFFER_OFFSET = 0x1000;
const uint16_t GCR_CHECKSUM_BUFFER_OFFSET = 0x1400;

extern bool enhancedisk;

void DiskInitialize();
void DiskDestroy();

void DiskBoot();

void DiskEject(const int iDrive);

const char* DiskGetFullName(int);

enum Disk_Status_e {
  DISK_STATUS_OFF, DISK_STATUS_READ, DISK_STATUS_WRITE, DISK_STATUS_PROT, NUM_DISK_STATUS
};

void DiskGetLightStatus(int *pDisk1Status_, int *pDisk2Status_);

const char* DiskGetName(int);

int DiskInsert(int, const char*, bool, bool);

bool DiskIsSpinning();

void DiskNotifyInvalidImage(const char*, int);

void DiskReset();

bool DiskGetProtect(const int iDrive);

void DiskSetProtect(const int iDrive, const bool bWriteProtect);

void DiskSelect(int);

void Disk_FTP_SelectImage(int);

void DiskUpdatePosition(uint32_t);

bool DiskDriveSwap();

uint32_t DiskGetSnapshot(SS_CARD_DISK2 *pSS, uint32_t dwSlot);

uint32_t DiskSetSnapshot(SS_CARD_DISK2 *pSS, uint32_t dwSlot);
