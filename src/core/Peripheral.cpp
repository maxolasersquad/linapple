#include "Peripheral.h"
#include "LinAppleCore.h"
#include "core/Common.h"
#include "core/Common_Globals.h"
#include "core/Log.h"
#include "apple2/Memory.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

// Internal structure to track an active peripheral instance
typedef struct {
    Peripheral_t* api;
    void* instance;
    int slot;
    PeripheralIOHandler readC0;
    PeripheralIOHandler writeC0;
    PeripheralIOHandler readCx;
    PeripheralIOHandler writeCx;
} ActivePeripheral_t;

static ActivePeripheral_t g_active_peripherals[NUM_SLOTS];

// --- I/O Proxy Functions ---

static uint8_t Peripheral_C0_Proxy(uint16_t pc, uint16_t addr, uint8_t write, uint8_t val, uint32_t cycles_left) {
    int slot = (addr >> 4) & 0x7;
    ActivePeripheral_t* p = &g_active_peripherals[slot];
    
    if (write) {
        if (p->writeC0) return p->writeC0(p->instance, pc, addr, write, val, cycles_left);
    } else {
        if (p->readC0) return p->readC0(p->instance, pc, addr, write, val, cycles_left);
    }
    
    return 0;
}

static uint8_t Peripheral_Cx_Proxy(uint16_t pc, uint16_t addr, uint8_t write, uint8_t val, uint32_t cycles_left) {
    int slot = (addr >> 8) & 0xF;
    if (slot >= NUM_SLOTS) return 0;
    ActivePeripheral_t* p = &g_active_peripherals[slot];
    
    if (write) {
        if (p->writeCx) return p->writeCx(p->instance, pc, addr, write, val, cycles_left);
    } else {
        if (p->readCx) return p->readCx(p->instance, pc, addr, write, val, cycles_left);
    }
    
    return 0;
}

// --- Host Interface Implementation ---

static void Host_Log(void* instance, PeripheralLogLevel level, const char* fmt, ...) {
    // Determine which peripheral is logging
    const char* name = "Unknown";
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (g_active_peripherals[i].instance == instance) {
            name = g_active_peripherals[i].api->name;
            break;
        }
    }

    va_list args;
    va_start(args, fmt);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    char final_buffer[1100];
    snprintf(final_buffer, sizeof(final_buffer), "[%s] %s", name, buffer);
    
    switch (level) {
        case LOG_DEBUG: Logger::Debug("%s", final_buffer); break;
        case LOG_INFO:  Logger::Info("%s", final_buffer); break;
        case LOG_WARN:  Logger::Warn("%s", final_buffer); break;
        case LOG_ERROR: Logger::Error("%s", final_buffer); break;
    }
    
    va_end(args);
}

static void Host_AssertIrq(int slot, bool assert) {
    (void)slot; (void)assert;
}

static void Host_RegisterIO(int slot, PeripheralIOHandler readC0, PeripheralIOHandler writeC0, 
                                     PeripheralIOHandler readCx, PeripheralIOHandler writeCx) {
    if (slot < 0 || slot >= NUM_SLOTS) return;
    
    g_active_peripherals[slot].readC0 = readC0;
    g_active_peripherals[slot].writeC0 = writeC0;
    g_active_peripherals[slot].readCx = readCx;
    g_active_peripherals[slot].writeCx = writeCx;
    
    RegisterIoHandler(slot, 
        readC0 ? (iofunction)Peripheral_C0_Proxy : NULL, 
        writeC0 ? (iofunction)Peripheral_C0_Proxy : NULL, 
        readCx ? (iofunction)Peripheral_Cx_Proxy : NULL, 
        writeCx ? (iofunction)Peripheral_Cx_Proxy : NULL, 
        NULL, NULL);
}

static void Host_RegisterCxROM(int slot, uint8_t* rom_ptr) {
    if (slot < 1 || slot >= NUM_SLOTS || !rom_ptr) return;
    
    uint8_t* pCxRomPeripheral = MemGetCxRomPeripheral();
    if (pCxRomPeripheral) {
        memcpy(pCxRomPeripheral + (slot * 0x100), rom_ptr, 256);
    }
}

static uint8_t* Host_GetMemPtr(uint16_t addr) {
    return mem + addr;
}

static uint64_t Host_GetCycles() {
    return cumulativecycles;
}

static HostInterface_t g_host_interface = {
    Host_Log,
    Host_AssertIrq,
    Host_RegisterIO,
    Host_RegisterCxROM,
    Host_GetMemPtr,
    Host_GetCycles
};

// --- Public Core API ---

void Peripheral_Manager_Init() {
    for (int i = 0; i < NUM_SLOTS; i++) {
        g_active_peripherals[i].api = NULL;
        g_active_peripherals[i].instance = NULL;
        g_active_peripherals[i].readC0 = NULL;
        g_active_peripherals[i].writeC0 = NULL;
        g_active_peripherals[i].readCx = NULL;
        g_active_peripherals[i].writeCx = NULL;
    }
}

void Peripheral_Manager_Reset() {
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (g_active_peripherals[i].api && g_active_peripherals[i].api->reset) {
            g_active_peripherals[i].api->reset(g_active_peripherals[i].instance);
        }
    }
}

void Peripheral_Manager_Shutdown() {
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (g_active_peripherals[i].api && g_active_peripherals[i].api->shutdown) {
            g_active_peripherals[i].api->shutdown(g_active_peripherals[i].instance);
        }
        g_active_peripherals[i].api = NULL;
        g_active_peripherals[i].instance = NULL;
    }
}

void Peripheral_Manager_Think(uint32_t cycles) {
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (g_active_peripherals[i].api && g_active_peripherals[i].api->think) {
            g_active_peripherals[i].api->think(g_active_peripherals[i].instance, cycles);
        }
    }
}

int Peripheral_Register(Peripheral_t* api, int slot) {
    if (!api || slot < 0 || slot >= NUM_SLOTS) return -1;
    
    if (api->abi_version != LINAPPLE_ABI_VERSION) {
        Logger::Error("Peripheral '%s' has incompatible ABI version %d (core expects %d)", 
                      api->name, api->abi_version, LINAPPLE_ABI_VERSION);
        return -1;
    }

    if (!(api->compatible_slots & (1 << slot))) {
        Logger::Error("Peripheral '%s' is not compatible with slot %d", api->name, slot);
        return -1;
    }

    void* instance = api->init(slot, &g_host_interface);
    if (!instance) {
        Logger::Error("Failed to initialize peripheral '%s' in slot %d", api->name, slot);
        return -1;
    }

    g_active_peripherals[slot].api = api;
    g_active_peripherals[slot].instance = instance;
    g_active_peripherals[slot].slot = slot;
    
    Logger::Info("Registered peripheral '%s' in slot %d", api->name, slot);
    return 0;
}
