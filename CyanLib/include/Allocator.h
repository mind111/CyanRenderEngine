#pragma once

#include "Common.h"

// no fragmentation within the memory managed by a StackAllocator
struct StackAllocator
{
    StackAllocator(u32 nBytes)
    {
        m_pos = 0u;
        m_size = nBytes;
        // TODO: alloc from memory pool instead
        m_bytes = new u8[nBytes];
        // clear to 0
        memset(m_bytes, 0x0, m_size);
    }

    ~StackAllocator()
    {
        delete[] m_bytes;
    }

    void* alloc(u32 size)
    {
        // 4 additional bytes for position marker stored at the end of each allocated region
        if (m_pos + (size + 4u) <= m_size)
        {
            m_numAllocations += 1;
            // cache the size of this allocation at the end of allocated region
            u32* markerPtr = reinterpret_cast<u32*>(&m_bytes[m_pos + size]);
            *markerPtr = m_pos;
            void* address = reinterpret_cast<void*>(m_bytes + m_pos); 
            m_pos += (size + 4u);
            return address;
        }
        CYAN_ASSERT(0, "StackAllocator out of memory");
        return nullptr;
    }

    // don't need to think about free for now
    void free()
    {
        if (m_pos > 0u)
        {
            m_numAllocations -= 1;
            u32* markerPtr = reinterpret_cast<u32*>(&m_bytes[m_pos - 4u]);
            u32 size = *markerPtr;
            m_pos -= (size + 4u);
        }
    }

    void reset()
    {
        m_pos = 0u;
    }

    u32 m_pos;
    u32 m_numAllocations;
    u32 m_size;
    u8* m_bytes;
};