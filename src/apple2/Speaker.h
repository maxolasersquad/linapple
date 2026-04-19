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

typedef struct Speaker_t {
  SpkrEvent events[MAX_SPKR_EVENTS];
  int num_events;
  bool state;
  uint64_t last_cycle;
  uint64_t quiet_cycle_count;
  bool recently_active;
  bool toggle_flag;
  void* host; // Opaque pointer to HostInterface_t
} Speaker_t;

// Legacy API (uses internal default instance)
auto SpkrDestroy() -> void;
auto SpkrInitialize() -> void;
auto SpkrReinitialize() -> void;
auto SpkrReset() -> void;
auto SpkrUpdate(uint32_t totalcycles) -> void;

auto Spkr_IsActive() -> bool;

auto SpkrGetSnapshot(SS_IO_Speaker *pSS) -> uint32_t;
auto SpkrSetSnapshot(SS_IO_Speaker *pSS) -> uint32_t;

auto SpkrToggle(void* instance, uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

// Pointer-based API
auto Speaker_Destroy(Speaker_t* instance) -> void;
auto Speaker_Initialize(Speaker_t* instance) -> void;
auto Speaker_Reset(Speaker_t* instance) -> void;
auto Speaker_Update(Speaker_t* instance, uint32_t totalcycles) -> void;
auto Speaker_IsActive(Speaker_t* instance) -> bool;
auto Speaker_Toggle(Speaker_t* instance, uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft) -> uint8_t;

// Core Speaker API for Frontend
auto SpkrGetEvents(SpkrEvent *events, int max_events) -> int;
auto SpkrGetLastCycle() -> uint64_t;
auto SpkrGetCurrentState() -> bool;

// Core Speaker API for Frontend (Pointer-based)
auto Speaker_GetEvents(Speaker_t* instance, SpkrEvent *events, int max_events) -> int;
auto Speaker_GetLastCycle(Speaker_t* instance) -> uint64_t;
auto Speaker_GetCurrentState(Speaker_t* instance) -> bool;
