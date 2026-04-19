#include <cstdint>
#include "apple2/Speaker_Structs.h"
#pragma once

typedef struct tagSS_IO_Speaker SS_IO_Speaker;

// For audio use none or SDL_SOUND subsystem
#define SOUND_NONE 0
#define SOUND_WAVE 1

#define SPKR_SAMPLE_VOLUME 0x4000

extern uint32_t soundtype;

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
auto Speaker_GenerateSamples(Speaker_t* instance, uint32_t dwExecutedCycles) -> void;

// Core Speaker API for Frontend
auto SpkrGetEvents(SpkrEvent *events, int max_events) -> int;
auto SpkrGetLastCycle() -> uint64_t;
auto SpkrGetCurrentState() -> bool;

// Core Speaker API for Frontend (Pointer-based)
auto Speaker_GetEvents(Speaker_t* instance, SpkrEvent *events, int max_events) -> int;
auto Speaker_GetLastCycle(Speaker_t* instance) -> uint64_t;
auto Speaker_GetCurrentState(Speaker_t* instance) -> bool;

// Frontend/Core compatibility layer (to be removed in Task 6.5)
auto SpkrFrontend_Reset() -> void;
auto SpkrFrontend_Update(uint32_t dwExecutedCycles) -> void;
