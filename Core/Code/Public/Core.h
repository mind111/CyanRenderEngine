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