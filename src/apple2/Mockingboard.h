#include <cstdint>
#pragma once

typedef struct tagSS_CARD_MOCKINGBOARD SS_CARD_MOCKINGBOARD;

extern bool g_bMBTimerIrqActive;
extern uint32_t g_uTimer1IrqCount;

void MB_Initialize();

void MB_Reinitialize();

void MB_Destroy();

void MB_Reset();

void MB_Mute();

void MB_Demute();

void MB_StartOfCpuExecute();

void MB_EndOfVideoFrame();

void MB_CheckIRQ();

void MB_UpdateCycles(uint32_t uExecutedCycles);

void MB_Update();

enum eSOUNDCARDTYPE {
  SC_UNINIT = 0,
  SC_NONE,
  SC_MOCKINGBOARD,
  SC_PHASOR
};  // Apple soundcard type

eSOUNDCARDTYPE MB_GetSoundcardType();

void MB_SetSoundcardType(eSOUNDCARDTYPE NewSoundcardType);

double MB_GetFramePeriod();

bool MB_IsActive();

uint32_t MB_GetVolume();

void MB_SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax);

uint32_t MB_GetSnapshot(SS_CARD_MOCKINGBOARD *pSS, uint32_t dwSlot);

uint32_t MB_SetSnapshot(SS_CARD_MOCKINGBOARD *pSS, uint32_t dwSlot);

