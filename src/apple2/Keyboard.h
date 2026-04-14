#include <cstdint>
#pragma once

typedef struct tagSS_IO_Keyboard SS_IO_Keyboard;

extern bool g_bShiftKey;
extern bool g_bCtrlKey;
extern bool g_bAltKey;

typedef enum {
  English_US=1,
  English_UK=2,
  French_FR=3,
  German_DE=4,
  Spanish_ES=5
} KeybLanguage;

extern KeybLanguage g_KeyboardLanguage;
extern bool         g_KeyboardRockerSwitch;

void KeybReset();
void KeybSetModifiers(bool bShift, bool bCtrl, bool bAlt);
void KeybPushAppleKey(uint8_t apple_code);
void KeybQueueKeypress(uint8_t apple_code);
void KeybSetCapsLock(bool bState);
void KeybSetAnyKeyDownStatus(bool bDown);
bool KeybGetAnyKeyDownStatus();

bool KeybGetAltStatus();
bool KeybGetCapsStatus();
bool KeybGetCtrlStatus();
bool KeybGetShiftStatus();
void KeybUpdateCtrlShiftStatus();
uint8_t KeybGetKeycode();

uint8_t KeybReadData(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);
uint8_t KeybReadFlag(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);

void ClipboardInitiatePaste();
uint32_t KeybGetNumQueries();
uint32_t KeybGetSnapshot(SS_IO_Keyboard *pSS);
uint32_t KeybSetSnapshot(SS_IO_Keyboard *pSS);

uint8_t KeybReadData(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);
uint8_t KeybReadFlag(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);
uint8_t KeybClearFlag(uint16_t pc, uint16_t addr, uint8_t bWrite, uint8_t d, uint32_t nCyclesLeft);
