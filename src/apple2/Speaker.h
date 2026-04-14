#include <cstdint>
#pragma once

typedef struct tagSS_IO_Speaker SS_IO_Speaker;

// For audio use none or SDL_SOUND subsystem
#define SOUND_NONE 0
#define SOUND_WAVE 1

#define MAX_SPKR_EVENTS 4096

extern uint32_t soundtype;

typedef struct {
  uint64_t cycle;
  bool state;
} SpkrEvent;

void SpkrDestroy();
void SpkrInitialize();
void SpkrReinitialize();
void SpkrReset();
void SpkrUpdate(uint32_t totalcycles);

bool Spkr_IsActive();

uint32_t SpkrGetSnapshot(SS_IO_Speaker *pSS);
uint32_t SpkrSetSnapshot(SS_IO_Speaker *pSS);

uint8_t SpkrToggle(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);

// Core Speaker API for Frontend
int SpkrGetEvents(SpkrEvent *events, int max_events);
uint64_t SpkrGetLastCycle();
bool SpkrGetCurrentState();
