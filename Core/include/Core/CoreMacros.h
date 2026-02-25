#pragma once
#include <cstdio>
#include <cstdlib>
#include <cassert>

//#include "Log.h"

namespace Rebel::Core
{
    // =======================================================
    // Debug / Fatal Utilities
    // =======================================================

#if defined(_MSC_VER)
    // MSVC debug break
    #define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang debug break via SIGTRAP
    #include <signal.h>
    #define DEBUG_BREAK() raise(SIGTRAP)
#else
    // Default no-op
    #define DEBUG_BREAK() ((void)0)
#endif

    /**
     * @brief Trigger a fatal error and abort program
     * @param msg Message describing the error
     * @param file File where the error occurred
     * @param line Line number
     */
    [[noreturn]] inline void FatalError(const char* msg, const char* file, int line)
    {
        std::fprintf(stderr, "Fatal Error: %s\nFile: %s:%d\n", msg, file, line);
        DEBUG_BREAK();
        std::abort();
    }
     //inline DEFINE_LOG_CATEGORY(ExacutionFlow)
    inline const char* GetFileName(const char* path)
    {
        const char* filename = path;
        for (const char* p = path; *p; ++p)
        {
            if (*p == '/' || *p == '\\') // handle both / and \ paths
                filename = p + 1;
        }
        return filename;
    }
    /**
     * @brief Trigger an ensure error (logs but continues execution)
     * @param msg Message describing the error
     * @param file File where the error occurred
     * @param line Line number
     */
    inline void EnsureError(const char* msg, const char* file, int line)
    {
        const char* filename = GetFileName(file);
        std::fprintf(stderr, "Ensure Failed: %s\nFile: %s:%d\n", msg, filename, line);
        DEBUG_BREAK();
    }

    /**
     * @brief Marks unreachable code (fatal if executed)
     * @param msg Message describing why this code is unreachable
     */
    [[noreturn]] inline void Unreachable(const char* msg, const char* file, int line)
    {
        std::fprintf(stderr, "Unreachable Code Reached: %s\nFile: %s:%d\n", msg, file, line);
        DEBUG_BREAK();
        std::abort();
    }
}

// =======================================================
// Compiler / Platform Hints
// =======================================================

#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline               // Always inline (MSVC)
    #define NOINLINE __declspec(noinline)          // Never inline
    #define DEPRECATED(msg) __declspec(deprecated(msg)) // Mark deprecated
    #define MAYBE_UNUSED                             // MSVC doesn't need special attribute
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCEINLINE inline __attribute__((always_inline)) // Force inline
    #define NOINLINE __attribute__((noinline))                 // Never inline
    #define DEPRECATED(msg) __attribute__((deprecated(msg)))  // Deprecation
    #define MAYBE_UNUSED __attribute__((unused))             // Avoid unused warnings
#else
    #define FORCEINLINE inline
    #define NOINLINE
    #define DEPRECATED(msg)
    #define MAYBE_UNUSED
#endif

// =======================================================
// Compile-time / Preprocessor Helpers
// =======================================================

#define STATIC_ASSERT(expr, msg) static_assert(expr, msg)  // Compile-time assertion
#define ARRAY_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))      // Count of static array elements
#define BIT(x) (1 << (x))                                  // Create bit flag (1 << x)
#define STR(x) #x                                          // Convert token to string
#define CONCAT(a,b) a##b                                   // Concatenate tokens
#define UNIQUE_NAME(base) CONCAT(base, __LINE__)           // Unique name based on line number
#define TODO(msg) static_assert(false, "TODO: " msg)       // Compile-time reminder

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1) // Likely branch
    #define UNLIKELY(x) __builtin_expect(!!(x), 0) // Unlikely branch
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// =======================================================
// Core Assertions
// =======================================================

// Logs error and triggers debug break, then continues execution
#define ENSURE(expr) \
([&]() -> bool { \
static bool bTriggered = false; \
if (!(expr)) { \
if (!bTriggered) { \
bTriggered = true; \
DEBUG_BREAK(); \
} \
Rebel::Core::EnsureError(#expr, __FILE__, __LINE__); \
return false; \
} \
return true; \
}())

// Logs error with custom message and triggers debug break, then continues execution
#define ENSURE_MSG(expr, msg) \
([&]() -> bool { \
static bool bTriggered = false; \
if (!(expr)) { \
if (!bTriggered) { \
bTriggered = true; \
DEBUG_BREAK(); \
} \
Rebel::Core::EnsureError(msg, __FILE__, __LINE__); \
return false; \
} \
return true; \
}())

// Fatal if expr is false, triggers debug break first
#define CHECK(expr) \
do { \
if (!(expr)) { \
DEBUG_BREAK(); \
Rebel::Core::FatalError(#expr, __FILE__, __LINE__); \
} \
} while(0)

// Fatal if expr is false with custom message, triggers debug break first
#define CHECK_MSG(expr, msg) \
do { \
if (!(expr)) { \
DEBUG_BREAK(); \
Rebel::Core::FatalError(msg, __FILE__, __LINE__); \
} \
} while(0)

// Debug-only checks
#ifndef NDEBUG
    #define VERIFY(expr) CHECK(expr)
    #define DEBUG_CHECK(expr) CHECK(expr)
    #define DEBUG_ENSURE(expr) ENSURE(expr)
#else
    #define VERIFY(expr) ((void)(expr))   // Remove checks in shipping build
    #define DEBUG_CHECK(expr) ((void)0)
    #define DEBUG_ENSURE(expr) ((void)0)
#endif

// =======================================================
// Misc / Helpers
// =======================================================

// Marks code as unreachable (fatal)
#define UNREACHABLE(msg) Rebel::Core::Unreachable(msg, __FILE__, __LINE__)

// Mark variable as unused to silence warnings
#define UNUSED(x) ((void)(x))

// Memory alignment helper (like UE_ALIGN)
#define ALIGNAS(x) alignas(x)

// Swap two integers using XOR
#define SWAP_XOR(a, b) do { \
if (&(a) != &(b)) {     /* Avoid swapping same memory */ \
(a) ^= (b);         \
(b) ^= (a);         \
(a) ^= (b);         \
}                       \
} while(0)

