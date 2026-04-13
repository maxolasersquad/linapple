/*
 * Peripheral.h - The LinApple Peripheral ABI
 *
 * This header defines the stable binary interface for emulator peripherals.
 * Peripherals implemented against this ABI can be statically linked or
 * loaded as shared objects.
 */

#ifndef LINAPPLE_PERIPHERAL_H
#define LINAPPLE_PERIPHERAL_H

#include <cstdint>
#include <cstddef>
#include <cstdarg>

#ifdef __cplusplus
extern "C" {
#endif

#define LINAPPLE_ABI_VERSION 1

/**
 * @brief Standard return codes for ABI functions.
 */
typedef enum {
    PERIPHERAL_OK = 0,
    PERIPHERAL_ERROR = -1,
    PERIPHERAL_INCOMPATIBLE = -2
} PeripheralStatus;

/**
 * @brief Log levels for peripherals.
 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} PeripheralLogLevel;

/**
 * @brief Standard I/O handler signature.
 */
typedef uint8_t (*PeripheralIOHandler)(void* instance, uint16_t pc, uint16_t addr, uint8_t write, uint8_t val, uint32_t cycles_left);

/**
 * @brief Services provided by the emulator core to the peripheral.
 */
typedef struct {
    void (*Log)(void* instance, PeripheralLogLevel level, const char* fmt, ...);
    void (*AssertIrq)(int slot, bool assert);
    void (*RegisterIO)(int slot, PeripheralIOHandler readC0, PeripheralIOHandler writeC0, 
                                 PeripheralIOHandler readCx, PeripheralIOHandler writeCx);
    void (*RegisterCxROM)(int slot, uint8_t* rom_ptr);
    uint8_t* (*GetMemPtr)(uint16_t addr);
    uint64_t (*GetCycles)();
} HostInterface_t;

/**
 * @brief The interface a peripheral must implement.
 */
typedef struct {
    int abi_version;
    const char* name;
    uint32_t compatible_slots;
    void* (*init)(int slot, HostInterface_t* host);
    void (*reset)(void* instance);
    void (*shutdown)(void* instance);
    void (*think)(void* instance, uint32_t cycles);
    PeripheralStatus (*save_state)(void* instance, void* buffer, size_t* size);
    PeripheralStatus (*load_state)(void* instance, const void* buffer, size_t size);
} Peripheral_t;

#ifdef __cplusplus
}
#endif

#endif // LINAPPLE_PERIPHERAL_H
