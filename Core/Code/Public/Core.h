#pragma once

#include <cstdint>
#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <array>
#include <functional>
#include <fstream>

#include "intrin.h"

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int      i64;
typedef float    f32;
typedef double   f64;

#ifdef CORE_EXPORTS
    #define CORE_API __declspec(dllexport)
#else
    #define CORE_API __declspec(dllimport)
#endif

#define NOT_IMPLEMENTED_ERROR() \
        assert(0);              \

#define UNREACHABLE_ERROR() \
        assert(0);          \

#define UNEXPECTED_FATAL_ERROR() \
        assert(0);               \

#define cyanInfo(...)           \
    {                           \
        printf("[Info] ");      \
        printf(__VA_ARGS__);    \
        printf("\n");           \
    }                           \

#define cyanError(str, ...)                     \
    {                                           \
        printf("\033[1;31m[Error] \033[0m");    \
        printf(str, __VA_ARGS__);               \
        printf("\n");                           \
    }                                           \

namespace Cyan
{
    template <typename T>
    u32 sizeOfVectorInBytes(const std::vector<T>& vector)
    {
        return (u32)(sizeof(T) * vector.size());
    }
}
