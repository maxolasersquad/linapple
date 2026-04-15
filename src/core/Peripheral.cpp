#include "Peripheral.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "LinAppleCore.h"
#include "apple2/CPU.h"
#include "apple2/Memory.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/Log.h"

// Constants for I/O ranges
static constexpr uint16_t IO_DIRECT_START = 0xC000;
static constexpr uint16_t IO_DIRECT_MASK = 0xFF80;
static constexpr uint8_t IO_DIRECT_INDEX_MASK = 0x7F;
static constexpr size_t IO_DIRECT_COUNT = 128;

// Internal structure to track an active peripheral instance
using ActivePeripheral_t = struct {
  Peripheral_t* api;
  void* instance;
  int slot;
  PeripheralIOHandler readC0;
  PeripheralIOHandler writeC0;
  PeripheralIOHandler readCx;
  PeripheralIOHandler writeCx;
  uint8_t* expansionRom;
};

// Justification: Peripheral Manager requires global state to track registered 
// hardware and dispatch I/O through static core callbacks.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<ActivePeripheral_t, NUM_SLOTS> g_active_peripherals;

// --- I/O Proxy Functions ---

static auto Peripheral_C0_Proxy(uint16_t pc, uint16_t addr, uint8_t write,
                                uint8_t val, uint32_t cycles_left) -> uint8_t {
  const uint16_t SLOT_ADDR_MASK = 0x7;
  const int slot = (addr >> 4) & SLOT_ADDR_MASK;
  ActivePeripheral_t& p = g_active_peripherals.at(static_cast<size_t>(slot));

  if (write) {
    if (p.writeC0) {
      return p.writeC0(p.instance, pc, addr, write, val, cycles_left);
    }
    return 0;
  } else {
    if (p.readC0) {
      return p.readC0(p.instance, pc, addr, write, val, cycles_left);
    }
  }

  return MemReadFloatingBus(cycles_left);
}

static auto Peripheral_Cx_Proxy(uint16_t pc, uint16_t addr, uint8_t write,
                                uint8_t val, uint32_t cycles_left) -> uint8_t {
  const uint16_t PAGE_ADDR_MASK = 0xF;
  const int slot = (addr >> 8) & PAGE_ADDR_MASK;
  if (slot >= NUM_SLOTS) {
    return write ? 0 : MemReadFloatingBus(cycles_left);
  }
  ActivePeripheral_t& p = g_active_peripherals.at(static_cast<size_t>(slot));

  if (write) {
    if (p.writeCx) {
      return p.writeCx(p.instance, pc, addr, write, val, cycles_left);
    }
    return 0;
  } else {
    if (p.readCx) {
      return p.readCx(p.instance, pc, addr, write, val, cycles_left);
    }
  }

  return MemReadFloatingBus(cycles_left);
}

// --- Host Interface Implementation ---

// Justification: The stable Peripheral ABI provides a C-style variadic logging 
// interface to simplify implementation for third-party modules.
// NOLINTNEXTLINE(modernize-avoid-variadic-functions)
static void Host_Log(void* instance, PeripheralLogLevel level, const char* fmt,
                     ...) {
  // Determine which peripheral is logging
  const char* name = "Unknown";
  for (size_t i = 0; i < NUM_SLOTS; ++i) {
    ActivePeripheral_t& ap = g_active_peripherals.at(i);
    if (ap.instance == instance) {
      name = ap.api->name;
      break;
    }
  }

  // Justification: Standard C variadic handling and local buffer management 
  // required to bridge the ABI and internal Logger.
  // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,
  // cppcoreguidelines-pro-bounds-array-to-pointer-decay)
  va_list args;
  va_start(args, fmt);

  const size_t LOG_BUFFER_SIZE = 1024;
  std::array<char, LOG_BUFFER_SIZE> buffer{};
  vsnprintf(buffer.data(), buffer.size(), fmt, args);

  const size_t FINAL_BUFFER_SIZE = LOG_BUFFER_SIZE + 76;  // Extra space for tag
  std::array<char, FINAL_BUFFER_SIZE> final_buffer{};
  snprintf(final_buffer.data(), final_buffer.size(), "[%s] %s", name,
           buffer.data());

  switch (level) {
    case LOG_DEBUG:
      Logger::Debug("%s", final_buffer.data());
      break;
    case LOG_INFO:
      Logger::Info("%s", final_buffer.data());
      break;
    case LOG_WARN:
      Logger::Warn("%s", final_buffer.data());
      break;
    case LOG_ERROR:
      Logger::Error("%s", final_buffer.data());
      break;
  }

  va_end(args);
  // NOLINTEND(cppcoreguidelines-pro-type-vararg,
  // cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

static void Host_AssertIrq(int slot, bool assert) {
  if (slot < 1 || slot >= NUM_SLOTS) {
    return;
  }

  auto source = static_cast<eIRQSRC>(static_cast<int>(IS_SLOT1) + (slot - 1));
  if (assert) {
    CpuIrqAssert(source);
  } else {
    CpuIrqDeassert(source);
  }
}

static void Host_RegisterIO(int slot, PeripheralIOHandler readC0,
                            PeripheralIOHandler writeC0,
                            PeripheralIOHandler readCx,
                            PeripheralIOHandler writeCx) {
  if (slot < 0 || slot >= NUM_SLOTS) {
    return;
  }

  ActivePeripheral_t& ap = g_active_peripherals.at(static_cast<size_t>(slot));
  ap.readC0 = readC0;
  ap.writeC0 = writeC0;
  ap.readCx = readCx;
  ap.writeCx = writeCx;

  RegisterIoHandler(
      static_cast<uint32_t>(slot),
      readC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr,
      writeC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr,
      readCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr,
      writeCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr, nullptr,
      ap.expansionRom);
}

static void Host_RegisterCxROM(int slot, uint8_t* rom_ptr) {
  if (slot < 1 || slot >= NUM_SLOTS || !rom_ptr) {
    return;
  }

  uint8_t* pCxRomPeripheral = MemGetCxRomPeripheral();
  if (pCxRomPeripheral) {
    // Justification: Peripheral ROMs are mapped into a contiguous memory block 
    // where each slot has its own page-aligned region.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    memcpy(pCxRomPeripheral + (static_cast<ptrdiff_t>(slot) *
                               static_cast<ptrdiff_t>(PAGE_SIZE)),
           rom_ptr, static_cast<size_t>(PAGE_SIZE));
  }
}

static void Host_RegisterExpansionROM(int slot, uint8_t* rom_ptr) {
  if (slot < 1 || slot >= NUM_SLOTS) {
    return;
  }

  ActivePeripheral_t& ap = g_active_peripherals.at(static_cast<size_t>(slot));
  ap.expansionRom = rom_ptr;

  RegisterIoHandler(
      static_cast<uint32_t>(slot),
      ap.readC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr,
      ap.writeC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr,
      ap.readCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr,
      ap.writeCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr,
      nullptr, ap.expansionRom);
}

// Track direct I/O handlers ($C000-$C07F)
using DirectIO_t = struct {
  void* instance;
  PeripheralIOHandler read;
  PeripheralIOHandler write;
};

// Justification: Global state required for rapid I/O dispatching outside of 
// standard slot address ranges (e.g., $C030 speaker).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<DirectIO_t, IO_DIRECT_COUNT> g_direct_io_handlers;

static auto Peripheral_Direct_Proxy(uint16_t pc, uint16_t addr, uint8_t write,
                                    uint8_t val, uint32_t cycles_left)
    -> uint8_t {
  const auto index = static_cast<uint8_t>(addr & IO_DIRECT_INDEX_MASK);
  DirectIO_t& d = g_direct_io_handlers.at(index);

  if (write) {
    if (d.write) {
      return d.write(d.instance, pc, addr, write, val, cycles_left);
    }
  } else {
    if (d.read) {
      return d.read(d.instance, pc, addr, write, val, cycles_left);
    }
  }

  return MemReadFloatingBus(cycles_left);
}

static void Host_RegisterDirectIO(void* instance, uint16_t addr,
                                  PeripheralIOHandler read,
                                  PeripheralIOHandler write) {
  if ((addr & IO_DIRECT_MASK) != IO_DIRECT_START) {
    return;  // Only support $C000-$C07F for now via this method
  }

  const auto index = static_cast<uint8_t>(addr & IO_DIRECT_INDEX_MASK);
  g_direct_io_handlers.at(index) = {instance, read, write};

  // Register the proxy with the core memory system
  // Justification: Direct array access into the legacy core I/O dispatch table.
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
  if (read) {
    IORead[index] = static_cast<iofunction>(Peripheral_Direct_Proxy);
  }
  if (write) {
    IOWrite[index] = static_cast<iofunction>(Peripheral_Direct_Proxy);
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
}

static auto Host_GetMemPtr(uint16_t addr) -> uint8_t* {
  // Justification: Raw pointer access to core emulator memory required for 
  // high-performance peripheral interaction.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return mem + static_cast<ptrdiff_t>(addr);
}

static auto Host_GetCycles() -> uint64_t { return cumulativecycles; }

// Justification: Global immutable dispatch table for services provided to 
// peripherals.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static const HostInterface_t g_host_interface = {
    Host_Log,           Host_AssertIrq,        Host_RegisterIO,
    Host_RegisterCxROM, Host_RegisterExpansionROM, Host_RegisterDirectIO, Host_GetMemPtr,
    Host_GetCycles};

// --- Public Core API ---

void Peripheral_Manager_Init() {
  for (size_t i = 0; i < NUM_SLOTS; ++i) {
    ActivePeripheral_t& ap = g_active_peripherals.at(i);
    ap.api = nullptr;
    ap.instance = nullptr;
    ap.readC0 = nullptr;
    ap.writeC0 = nullptr;
    ap.readCx = nullptr;
    ap.writeCx = nullptr;
    ap.expansionRom = nullptr;
  }

  for (size_t i = 0; i < IO_DIRECT_COUNT; ++i) {
    g_direct_io_handlers.at(i) = {nullptr, nullptr, nullptr};
  }
}

void Peripheral_Manager_Reset() {
  for (size_t i = 0; i < NUM_SLOTS; ++i) {
    ActivePeripheral_t& ap = g_active_peripherals.at(i);
    if (ap.api && ap.api->reset) {
      ap.api->reset(ap.instance);
    }
  }
}

void Peripheral_Manager_Shutdown() {
  for (size_t i = 0; i < NUM_SLOTS; ++i) {
    ActivePeripheral_t& ap = g_active_peripherals.at(i);
    if (ap.api && ap.api->shutdown) {
      ap.api->shutdown(ap.instance);
    }
    ap.api = nullptr;
    ap.instance = nullptr;
  }
}

void Peripheral_Manager_Think(uint32_t cycles) {
  for (size_t i = 0; i < NUM_SLOTS; ++i) {
    ActivePeripheral_t& ap = g_active_peripherals.at(i);
    if (ap.api && ap.api->think) {
      ap.api->think(ap.instance, cycles);
    }
  }
}

auto Peripheral_Register(Peripheral_t* api, int slot) -> int {
  if (!api || slot < 0 || slot >= NUM_SLOTS) {
    return -1;
  }

  if (api->abi_version != LINAPPLE_ABI_VERSION) {
    // Justification: Logger uses standard C variadics.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    Logger::Error(
        "Peripheral '%s' has incompatible ABI version %d (core expects %d)",
        api->name, api->abi_version, LINAPPLE_ABI_VERSION);
    return -1;
  }

  if (!(api->compatible_slots & (1u << static_cast<uint32_t>(slot)))) {
    // Justification: Logger uses standard C variadics.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    Logger::Error("Peripheral '%s' is not compatible with slot %d", api->name,
                  slot);
    return -1;
  }

  // Justification: The Peripheral ABI is a C interface; the HostInterface must 
  // be passed as a non-const pointer to allow peripherals to store it, but we 
  // provide a central const implementation.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  void* instance = api->init(slot, const_cast<HostInterface_t*>(&g_host_interface));
  if (!instance) {
    // Justification: Logger uses standard C variadics.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    Logger::Error("Failed to initialize peripheral '%s' in slot %d", api->name,
                  slot);
    return -1;
  }

  ActivePeripheral_t& ap = g_active_peripherals.at(static_cast<size_t>(slot));
  ap.api = api;
  ap.instance = instance;
  ap.slot = slot;

  // Justification: Logger uses standard C variadics.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  Logger::Info("Registered peripheral '%s' in slot %d\n", api->name, slot);
  return 0;
}
