#ifndef DISKIMAGE_H
#define DISKIMAGE_H

#include <cstdint>

#pragma once

typedef void* DiskImagePtr_t;
typedef DiskImagePtr_t HIMAGE;

constexpr int IMAGETYPES           = 8;
constexpr int WOZ2_HEADER_SIZE     = 1536; /* three 512-byte blocks */
constexpr int WOZ2_DATA_BLOCK_SIZE  = 512;
constexpr int WOZ2_TMAP_OFFSET       = 88; /* bytes from beginning of WOZ2 file */
constexpr int WOZ2_TMAP_SIZE        = 160;
constexpr int WOZ2_TRKS_OFFSET      = 256; /* bytes from beginning of WOZ2 file */
constexpr int WOZ2_TRKS_MAX_SIZE    = 160;
constexpr int WOZ2_TRK_SIZE           = 8;

bool ImageBoot(DiskImagePtr_t);
void ImageClose(DiskImagePtr_t);

void ImageDestroy();

void ImageInitialize();

enum ImageError_e {
  IMAGE_ERROR_BAD_POINTER = -1, IMAGE_ERROR_NONE = 0, IMAGE_ERROR_UNABLE_TO_OPEN = 1, IMAGE_ERROR_BAD_SIZE = 2
};

int ImageOpen(const char* imagefilename, DiskImagePtr_t *hDiskImage_, bool *pWriteProtected_, bool bCreateIfNecessary);

void ImageReadTrack(DiskImagePtr_t, int, int, uint8_t*, int *);

void ImageWriteTrack(DiskImagePtr_t, int, int, uint8_t*, int);

#endif // DISKIMAGE_H
