#include "Peripheral.h"
#include "LinAppleCore.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/Log.h"
#include "apple2/Memory.h"
#include <cstddef>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <array>

// Internal structure to track an active peripheral instance
using ActivePeripheral_t = struct {
    Peripheral_t* api;
    void* instance;
    int slot;
    PeripheralIOHandler readC0;
    PeripheralIOHandler writeC0;
    PeripheralIOHandler readCx;
    PeripheralIOHandler writeCx;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): Global manager state
static std::array<ActivePeripheral_t, NUM_SLOTS> g_active_peripherals;

// --- I/O Proxy Functions ---

static auto Peripheral_C0_Proxy(uint16_t pc, uint16_t addr, uint8_t write, uint8_t val, uint32_t cycles_left) -> uint8_t {
    const uint16_t SLOT_ADDR_MASK = 0x7;
    const int slot = (addr >> 4) & SLOT_ADDR_MASK;
    ActivePeripheral_t& p = g_active_peripherals.at(static_cast<size_t>(slot));
    
    if (write) {
        if (p.writeC0) {
            return p.writeC0(p.instance, pc, addr, write, val, cycles_left);
        }
    } else {
        if (p.readC0) {
            return p.readC0(p.instance, pc, addr, write, val, cycles_left);
        }
    }
    
    return 0;
}

static auto Peripheral_Cx_Proxy(uint16_t pc, uint16_t addr, uint8_t write, uint8_t val, uint32_t cycles_left) -> uint8_t {
    const uint16_t PAGE_ADDR_MASK = 0xF;
    const int slot = (addr >> 8) & PAGE_ADDR_MASK;
    if (slot >= NUM_SLOTS) {
        return 0;
    }
    ActivePeripheral_t& p = g_active_peripherals.at(static_cast<size_t>(slot));
    
    if (write) {
        if (p.writeCx) {
            return p.writeCx(p.instance, pc, addr, write, val, cycles_left);
        }
    } else {
        if (p.readCx) {
            return p.readCx(p.instance, pc, addr, write, val, cycles_left);
        }
    }
    
    return 0;
}

// --- Host Interface Implementation ---

// NOLINTNEXTLINE(modernize-avoid-variadic-functions): Peripheral logging interface
static void Host_Log(void* instance, PeripheralLogLevel level, const char* fmt, ...) {
    // Determine which peripheral is logging
    const char* name = "Unknown";
    for (size_t i = 0; i < NUM_SLOTS; ++i) {
        ActivePeripheral_t& ap = g_active_peripherals.at(i);
        if (ap.instance == instance) {
            name = ap.api->name;
            break;
        }
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    va_list args;
    va_start(args, fmt);
    
    const size_t LOG_BUFFER_SIZE = 1024;
    std::array<char, LOG_BUFFER_SIZE> buffer{};
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    
    const size_t FINAL_BUFFER_SIZE = LOG_BUFFER_SIZE + 76; // Extra space for tag
    std::array<char, FINAL_BUFFER_SIZE> final_buffer{};
    snprintf(final_buffer.data(), final_buffer.size(), "[%s] %s", name, buffer.data());
    
    switch (level) {
        case LOG_DEBUG: Logger::Debug("%s", final_buffer.data()); break;
        case LOG_INFO:  Logger::Info("%s", final_buffer.data()); break;
        case LOG_WARN:  Logger::Warn("%s", final_buffer.data()); break;
        case LOG_ERROR: Logger::Error("%s", final_buffer.data()); break;
    }
    
    va_end(args);
    // NOLINTEND(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

static void Host_AssertIrq(int slot, bool assert) {
    (void)slot; (void)assert;
}

static void Host_RegisterIO(int slot, PeripheralIOHandler readC0, PeripheralIOHandler writeC0, 
                                     PeripheralIOHandler readCx, PeripheralIOHandler writeCx) {
    if (slot < 0 || slot >= NUM_SLOTS) {
        return;
    }
    
    ActivePeripheral_t& ap = g_active_peripherals.at(static_cast<size_t>(slot));
    ap.readC0 = readC0;
    ap.writeC0 = writeC0;
    ap.readCx = readCx;
    ap.writeCx = writeCx;
    
    RegisterIoHandler(static_cast<uint32_t>(slot), 
        readC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr, 
        writeC0 ? static_cast<iofunction>(Peripheral_C0_Proxy) : nullptr, 
        readCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr, 
        writeCx ? static_cast<iofunction>(Peripheral_Cx_Proxy) : nullptr, 
        nullptr, nullptr);
}

static void Host_RegisterCxROM(int slot, uint8_t* rom_ptr) {
    if (slot < 1 || slot >= NUM_SLOTS || !rom_ptr) {
        return;
    }
    
    uint8_t* pCxRomPeripheral = MemGetCxRomPeripheral();
    if (pCxRomPeripheral) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): Peripheral ROM offset
        memcpy(pCxRomPeripheral + (static_cast<ptrdiff_t>(slot) * static_cast<ptrdiff_t>(PAGE_SIZE)), rom_ptr, static_cast<size_t>(PAGE_SIZE));
    }
}

static auto Host_GetMemPtr(uint16_t addr) -> uint8_t* {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): Core memory access requires offset from base
    return mem + static_cast<ptrdiff_t>(addr);
}

static auto Host_GetCycles() -> uint64_t {
    return cumulativecycles;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): Global host interface
static const HostInterface_t g_host_interface = {
    Host_Log,
    Host_AssertIrq,
    Host_RegisterIO,
    Host_RegisterCxROM,
    Host_GetMemPtr,
    Host_GetCycles
};

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
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): Logger uses varargs
        Logger::Error("Peripheral '%s' has incompatible ABI version %d (core expects %d)", 
                      api->name, api->abi_version, LINAPPLE_ABI_VERSION);
        return -1;
    }

    if (!(api->compatible_slots & (1u << static_cast<uint32_t>(slot)))) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): Logger uses varargs
        Logger::Error("Peripheral '%s' is not compatible with slot %d", api->name, slot);
        return -1;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): Host interface needs non-const for pointer storage
    void* instance = api->init(slot, const_cast<HostInterface_t*>(&g_host_interface));
    if (!instance) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): Logger uses varargs
        Logger::Error("Failed to initialize peripheral '%s' in slot %d", api->name, slot);
        return -1;
    }

    ActivePeripheral_t& ap = g_active_peripherals.at(static_cast<size_t>(slot));
    ap.api = api;
    ap.instance = instance;
    ap.slot = slot;
    
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): Logger uses varargs
    Logger::Info("Registered peripheral '%s' in slot %d", api->name, slot);
    return 0;
}
