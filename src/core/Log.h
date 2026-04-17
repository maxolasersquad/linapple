#pragma once

#include <cstdarg>
#include <cstddef>

enum class LogLevel {
  kSilent = 0,
  kError,
  kWarning,
  kInfo,
  kPerf,
  kDebug
};

/**
 * @brief Callback type for external logging.
 */
using LogCallback = void (*)(LogLevel level, const char* message);

namespace Logger {

// Maximum size for stack-allocated log buffers before falling back to heap.
constexpr size_t kMaxStackLogSize = 1024;

void Initialize();
void Destroy();

void SetVerbosity(LogLevel level);
void SetCallback(LogCallback callback);

/**
 * @brief Core logging functions with GCC/Clang format string checking.
 */
#if defined(__GNUC__) || defined(__clang__)
#define ATTRIBUTE_FORMAT_PRINTF(fmt, first) __attribute__((format(printf, fmt, first)))
#else
#define ATTRIBUTE_FORMAT_PRINTF(fmt, first)
#endif

void Error(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
void Warning(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
void Info(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
void Perf(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
void Debug(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

#undef ATTRIBUTE_FORMAT_PRINTF

/**
 * @brief Internal implementation helper using va_list.
 */
void LogMessageV(LogLevel level, const char* format, va_list args);

}  // namespace Logger

/**
 * @brief Zero-cost LOG macro for Debug builds.
 */
#if defined(DEBUG) || defined(_DEBUG)
#define LOG(format, ...) Logger::Debug(format, ##__VA_ARGS__)
#else
#define LOG(format, ...) ((void)0)
#endif
