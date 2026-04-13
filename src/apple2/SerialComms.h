#ifndef SERIALCOMMS_H
#define SERIALCOMMS_H

#include <cstdint>
#include "apple2/Structs.h"

// SSC DIPSW structure
enum FirmwareMode {
  FIRMWARE_CIC = 0, FIRMWARE_SIC_P8, FIRMWARE_PPC, FIRMWARE_SIC_P8A
};

enum SscParity {
  SSC_PARITY_NONE = 0,
  SSC_PARITY_ODD = 1,
  SSC_PARITY_EVEN = 2,
  SSC_PARITY_MARK = 3,
  SSC_PARITY_SPACE = 4
};

enum SscStopBits {
  SSC_STOP_BITS_1 = 0,
  SSC_STOP_BITS_1_5 = 1,
  SSC_STOP_BITS_2 = 2
};

typedef struct SSC_DIPSW_tag {
  //DIPSW1
  unsigned int uBaudRate;
  FirmwareMode eFirmwareMode;

  //DIPSW2
  SscStopBits eStopBits;
  unsigned int uByteSize;
  SscParity eParity;
  bool bLinefeed;
  bool bInterrupts;
} SSC_DIPSW;

// Internal baud rate constants
#define SSC_B110    110
#define SSC_B300    300
#define SSC_B600    600
#define SSC_B1200   1200
#define SSC_B2400   2400
#define SSC_B4800   4800
#define SSC_B9600   9600
#define SSC_B19200  19200

// SuperSerialCard core state
struct SuperSerialCard {
  SSC_DIPSW m_DIPSWCurrent;

  // Derived from DIPSWs
  unsigned int m_uBaudRate;
  SscStopBits m_eStopBits;
  unsigned int m_uByteSize;
  SscParity m_eParity;

  // SSC Registers
  unsigned char m_uControlByte;
  unsigned char m_uCommandByte;

  // State
  unsigned char m_RecvBuffer[uRecvBufferSize];
  volatile unsigned int m_vRecvBytes;

  bool m_bTxIrqEnabled;
  bool m_bRxIrqEnabled;

  bool m_bWrittenTx;
  volatile bool m_vbCommIRQ;

  unsigned char *m_pExpansionRom;
};

// Procedural functions for the core logic
void SSC_Reset(SuperSerialCard* pSSC);
void SSC_Initialize(SuperSerialCard* pSSC, uint8_t* pCxRomPeripheral, unsigned int uSlot);
void SSC_Destroy(SuperSerialCard* pSSC);

// Snapshot
auto SSC_GetSnapshot(SuperSerialCard* pSSC, SS_IO_Comms *pSS) -> unsigned int;
auto SSC_SetSnapshot(SuperSerialCard* pSSC, SS_IO_Comms *pSS) -> unsigned int;

// IO Handlers
auto SSC_IORead(unsigned short PC, unsigned short uAddr, unsigned char bWrite, unsigned char uValue, uint32_t nCyclesLeft) -> unsigned char;
auto SSC_IOWrite(unsigned short PC, unsigned short uAddr, unsigned char bWrite, unsigned char uValue, uint32_t nCyclesLeft) -> unsigned char;

// Interface for Frontend to Core
void SSC_PushRxByte(SuperSerialCard* pSSC, uint8_t byte);

// Interface for Core to call Frontend (implemented in Frontend)
extern void SSCFrontend_SendByte(uint8_t byte);
extern auto SSCFrontend_IsActive() -> bool;
extern void SSCFrontend_UpdateState(unsigned int baud, unsigned int bits,
                                           SscParity parity, SscStopBits stop);

// Global instance (to be moved/handled)
extern struct SuperSerialCard sg_SSC;

#endif // SERIALCOMMS_H
