#include "core/Common.h"
#include <cstdio>
#include <cstdint>
#include "frontends/sdl3/PrinterFrontend.h"
#include "core/Common_Globals.h"

static unsigned int inactivity = 0;
static unsigned int g_PrinterIdleLimit = 10;
static FILE *file = NULL;
bool g_bPrinterAppend = true;

static auto CheckPrint() -> bool
{
  inactivity = 0;
  if (file == nullptr) {
    file = fopen(g_state.sParallelPrinterFile, (g_bPrinterAppend) ? "ab" : "wb");
  }
  return (file != nullptr);
}

static void ClosePrint() {
  if (file != nullptr) {
    fclose(file);
    file = nullptr;
  }
  inactivity = 0;
}

void PrinterFrontend_Initialize() {
  // Initialization logic if any
}

void PrinterFrontend_Destroy() {
  ClosePrint();
}

void PrinterFrontend_Reset() {
  ClosePrint();
}

void PrinterFrontend_Update(unsigned int totalcycles) {
  if (file == nullptr) {
    return;
  }
  if ((inactivity += totalcycles) > (Printer_GetIdleLimit() * 1000 * 1000))
  {
    // inactive, so close the file (next print will overwrite it)
    ClosePrint();
  }
}

void PrinterFrontend_SendChar(uint8_t value) {
  if (!CheckPrint()) {
    return;
  }
  char c = value & 0x7F;
  fwrite(&c, 1, 1, file);
}

void PrinterFrontend_CheckStatus() {
  CheckPrint();
}

auto Printer_GetIdleLimit() -> unsigned int {
  return g_PrinterIdleLimit;
}

void Printer_SetIdleLimit(unsigned int Duration) {
  g_PrinterIdleLimit = Duration;
}
