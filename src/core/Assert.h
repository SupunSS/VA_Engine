#pragma once
#include "Log.h"
#include <cstdlib>

#ifdef _MSC_VER
    #define DEBUG_BREAK() __debugbreak()
#else
    #define DEBUG_BREAK() std::abort()
#endif

#define ENGINE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            Log::Error("Assertion failed: {} ({}:{})", message, __FILE__, __LINE__); \
            DEBUG_BREAK(); \
        } \
    } while (0)