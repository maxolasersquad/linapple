#ifndef __asset_h
#define __asset_h

#include "apple2/Video.h"

using assets_t = struct assets_tag {
  void         *icon; // Platform-specific icon handle
  VideoSurface *font;
  VideoSurface *splash;
};

extern assets_t *assets;

auto Asset_Init() -> bool;

void Asset_Quit();

auto Asset_InsertMasterDisk() -> int;

#endif
