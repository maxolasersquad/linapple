#include "core/Common.h"
#include <iostream>
#include <cstdint>
#include <array>
#include "apple2/Joystick.h"
#include "apple2/Structs.h"
#include "apple2/Memory.h"
#include "apple2/CPU.h"
#include "core/Common_Globals.h"
#include "core/Log.h"

enum {
BUTTONTIME =       5000
};

enum {
DEVICE_NONE =      0,
DEVICE_JOYSTICK =  1,
DEVICE_KEYBOARD =  2,
DEVICE_MOUSE =     3
};

enum {
MODE_NONE =        0,
MODE_STANDARD =    1,
MODE_CENTERING =   2,
MODE_SMOOTH =      3
};

using joyinforec = struct joyinforec {
  int device;
  int mode;
};
using joyinfoptr = joyinforec*;

static const std::array<joyinforec, 5> joyinfo = {{
  {DEVICE_NONE,     MODE_NONE},
  {DEVICE_JOYSTICK, MODE_STANDARD},
  {DEVICE_KEYBOARD, MODE_STANDARD},
  {DEVICE_KEYBOARD, MODE_CENTERING},
  {DEVICE_MOUSE,    MODE_STANDARD}
}};

std::array<uint32_t, 2> joytype = {{DEVICE_JOYSTICK, DEVICE_NONE}};  // Emulation Type for joysticks #0 & #1

static std::array<uint32_t, 3> buttonlatch = {{0, 0, 0}};
static std::array<bool, 3> joybutton = {{false, false, false}};

static std::array<int, 2> xpos = {{127, 127}};
static std::array<int, 2> ypos = {{127, 127}};

static uint64_t g_nJoyCntrResetCycle = 0;  // Abs cycle that joystick counters were reset

static int g_nPdlTrimX = 0;
static int g_nPdlTrimY = 0;

uint32_t joy1index = 0;
uint32_t joy2index = 1;
uint32_t joy1button1 = 0;
uint32_t joy1button2 = 1;
uint32_t joy2button1 = 0;
uint32_t joy1axis0 = 0;
uint32_t joy1axis1 = 1;
uint32_t joy2axis0 = 0;
uint32_t joy2axis1 = 1;
uint32_t joyexitenable = 0;
uint32_t joyexitbutton0 = 8;
uint32_t joyexitbutton1 = 9;
bool joyquitevent = false;

// All globally accessible functions are below this line

void JoyShutDown() {
}

void JoyInitialize() {
}

void JoyReset() {
  for (int i = 0; i < 3; i++) {
    buttonlatch[static_cast<size_t>(i)] = 0;
    joybutton[static_cast<size_t>(i)] = false;
  }
  for (int i = 0; i < 2; i++) {
    xpos[static_cast<size_t>(i)] = 127;
    ypos[static_cast<size_t>(i)] = 127;
  }
}

auto JoyReadButton(uint16_t, uint16_t address, uint8_t, uint8_t, uint32_t nCyclesLeft) -> uint8_t {
  address &= 0xFF;
  bool pressed = false;
  int idx = address - 0x61;
  if (idx >= 0 && idx < 3) {
    pressed = (buttonlatch[static_cast<size_t>(idx)] || joybutton[static_cast<size_t>(idx)]);
    buttonlatch[static_cast<size_t>(idx)] = 0;
  }
  return MemReadFloatingBus(pressed, nCyclesLeft);
}

// PREAD:    ; $FB1E
//  AD 70 C0 : (4) LDA $C070
//  A0 00    : (2) LDA #$00
//  EA       : (2) NOP
//  EA       : (2) NOP
// Lbl1:            ; 11 cycles is the normal duration of the loop
//  BD 64 C0 : (4) LDA $C064,X
//  10 04    : (2) BPL Lbl2    ; NB. 3 cycles if branch taken (not likely)
//  C8       : (2) INY
//  D0 F8    : (3) BNE Lbl1    ; NB. 2 cycles if branck not taken (not likely)
//  88       : (2) DEY
// Lbl2:
//  60       : (6) RTS

static const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;  // 11.04 (From KEGS)

auto JoyReadPosition(uint16_t programcounter, uint16_t address, uint8_t, uint8_t, uint32_t nCyclesLeft) -> uint8_t {
  (void)programcounter;
  int nJoyNum = (address & 2) ? 1 : 0;  // $C064..$C067

  CpuCalcCycles(nCyclesLeft);

  uint32_t nPdlPos = (address & 1) ? static_cast<uint32_t>(ypos[static_cast<size_t>(nJoyNum)]) : static_cast<uint32_t>(xpos[static_cast<size_t>(nJoyNum)]);

  bool nPdlCntrActive = (g_nCumulativeCycles <= (g_nJoyCntrResetCycle + static_cast<uint64_t>(static_cast<double>(nPdlPos) * PDL_CNTR_INTERVAL)));

  return MemReadFloatingBus(nPdlCntrActive, nCyclesLeft);
}

auto JoyResetPosition(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t nCyclesLeft) -> uint8_t {
  CpuCalcCycles(nCyclesLeft);
  g_nJoyCntrResetCycle = g_nCumulativeCycles;
  return MemReadFloatingBus(nCyclesLeft);
}

void JoySetRawPosition(int joy, int x, int y) {
  if (joy >= 0 && joy < 2) {
    xpos[static_cast<size_t>(joy)] = x;
    ypos[static_cast<size_t>(joy)] = y;
  }
}

void JoySetRawButton(int button_idx, bool down) {
  if (button_idx >= 0 && button_idx < 3) {
    if (down && !joybutton[static_cast<size_t>(button_idx)]) {
      buttonlatch[static_cast<size_t>(button_idx)] = BUTTONTIME;
    }
    joybutton[static_cast<size_t>(button_idx)] = down;
  }
}

void JoyUpdatePosition(uint32_t dwExecutedCycles) {
  (void)dwExecutedCycles;
  for (uint32_t & i : buttonlatch) {
    if (i) {
      --i;
    }
  }
}

auto JoyGetSnapshot(SS_IO_Joystick *pSS) -> uint32_t {
  pSS->g_nJoyCntrResetCycle = g_nJoyCntrResetCycle;
  return 0;
}

auto JoySetSnapshot(SS_IO_Joystick *pSS) -> uint32_t {
  g_nJoyCntrResetCycle = pSS->g_nJoyCntrResetCycle;
  return 0;
}

void JoySetButton(eBUTTON number, eBUTTONSTATE down) {
  JoySetRawButton(static_cast<int>(number), down == BUTTON_DOWN);
}

void JoySetPosition(int xvalue, int xrange, int yvalue, int yrange) {
  if (xrange == 0 || yrange == 0) return;
  JoySetRawPosition(0, (xvalue * 255) / xrange, (yvalue * 255) / yrange);
}

auto JoySetEmulationType(uint32_t newType, int nJoystickNumber) -> bool {
  if (nJoystickNumber >= 0 && nJoystickNumber < 2) {
    joytype[static_cast<size_t>(nJoystickNumber)] = newType;
    return true;
  }
  return false;
}

auto JoyUsingMouse() -> bool {
  return (joyinfo[static_cast<size_t>(joytype[0])].device == DEVICE_MOUSE) || (joyinfo[static_cast<size_t>(joytype[1])].device == DEVICE_MOUSE);
}

void JoySetTrim(short nValue, bool bAxisX) {
  if (bAxisX) {
    g_nPdlTrimX = nValue;
  } else {
    g_nPdlTrimY = nValue;
  }
}

auto JoyGetTrim(bool bAxisX) -> short {
  return bAxisX ? g_nPdlTrimX : g_nPdlTrimY;
}
