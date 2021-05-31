// Common macros
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include "intrin.h"

// macros
#define CYAN_ASSERT(expr, ...) \
    if (!(expr)) {             \
        printf(__VA_ARGS__);   \
        __debugbreak();        \
    }

#define BREAK_WHEN(expr)     \
    if (expr)                \
    {                        \
        break;               \
    }                        \

#define EXEC_ONCE(call)       \
   static u32 numExecutions = 0; \
    if (numExecutions < 1)    \
    {                         \
        call;                 \
        numExecutions++;      \
    }                         \

#define ARRAY_COUNT(arr)       \
    sizeof(arr) / sizeof(arr[0])

// TODO: Is this macro safe ...?
#define Min(a, b) ((a < b) ? a : b)
#define Max(a, b) ((a > b) ? a : b)

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int      i64;
typedef float    f32;

typedef u32      UniformHandle;


