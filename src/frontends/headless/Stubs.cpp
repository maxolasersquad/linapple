#include "apple2/SerialComms.h"
#include <cstdint>

// These functions are currently coupled to the frontend in LinAppleCore.cpp or hardware files
// but should eventually be part of the bridge or a specialized frontend component.

bool g_bDSAvailable = false;

void SSCFrontend_Update(struct SuperSerialCard* pSSC, uint32_t cycles) { (void)pSSC; (void)cycles; }
void PrinterFrontend_Update(uint32_t cycles) { (void)cycles; }

// Printer Frontend stubs
auto PrinterFrontend_CheckStatus() -> uint8_t { return 0; }
void PrinterFrontend_SendChar(uint8_t c) { (void)c; }
void PrinterFrontend_Destroy() {}

// SSC Frontend stubs
void SSCFrontend_SendByte(uint8_t byte) { (void)byte; }
auto SSCFrontend_IsActive() -> bool { return false; }
void SSCFrontend_UpdateState(uint32_t baud, uint32_t bits, SscParity parity, SscStopBits stop) {
    (void)baud; (void)bits; (void)parity; (void)stop;
}

// UI stubs
void FrameRefreshStatus(int flags) { (void)flags; }
void DrawFrameWindow() {}
void DrawAppleContent() {}

// Legacy sound stubs
void DSInit() {}
void DSShutdown() {}

// Debugger stubs (normally in SDL_Video.cpp)
void StretchBltMemToFrameDC() {}

void SoundCore_SetFade(int fade) { (void)fade; }
void PrinterFrontend_Reset() {}
