// Common macros
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <queue>
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

#define BytesOffset(ptr, x) (reinterpret_cast<u8*>(ptr) + x)

#define cyanInfo(...)           \
    {                           \
        printf("[Info] ");      \
        printf(__VA_ARGS__);    \
        printf("\n");           \
    }                           \

#define cyanError(str, ...)                     \
    {                                           \
        printf("\033[1;31m[Error] \033[0m");     \
        printf(str, __VA_ARGS__);               \
        printf("\n");                           \
    }                                           \

#define TO_STRING(x) #x
#define SIZE_OF_VECTOR(x) (x.empty() ? 0 : x.size() * sizeof(x[0]))


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
typedef double   f64;

typedef u32      UniformHandle;

// TODO: pass in custom comparator
template <typename T>
T* treeBFS(T* node, const char* name)
{
    std::queue<T*> queue;
    queue.push(node);
    while (!queue.empty())
    {
        T* node = queue.front();
        queue.pop();
        if (node->name == name)
        {
            return node;
        }
        for (T* child : node->childs)
        {
            queue.push(child);
        }
    }
    return nullptr;
}

namespace Cyan
{
    template <typename T>
    u32 sizeOfVector(const std::vector<T>& vec)
    {
        if (vec.size() == 0)
        {
            return 0;
        }
        return sizeof(vec[0]) * (u32)vec.size();
    }

    template <typename T>
    class Singleton
    {
    public:
        Singleton() 
        {
            if (!singleton)
            {
                // downcast to derived type
                singleton = static_cast<T*>(this);
            }
        }

        Singleton(const Singleton& s) = delete;

        static T* get()
        { 
            return singleton; 
        }

    protected:
        static T* singleton;
    };
}

