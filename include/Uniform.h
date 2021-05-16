#pragma once

#include "Common.h"

const int kMaxUniformNameLen = 64;

struct Uniform
{
    enum Type
    {
        u_float = 0,
        u_int,
        u_vec3,
        u_vec4,
        u_mat4,
        u_sampler2D,
        u_samplerCube,
        u_undefined // Error
    };

    u32 getSize();

    Type m_type;
    char m_name[kMaxUniformNameLen];
    void* m_valuePtr;
};

struct UniformBuffer
{
    u32 m_size;
    u32 m_pos;
    void* m_data;

    void* read(u32 size)
    {
        CYAN_ASSERT(m_pos + size <= m_size, "Read from UniformBuffer out of bound")
        void* data = (void*)((u8*)m_data + m_pos);
        m_pos += size;
        return data;
    }

    void write(void* data, u32 size)
    {
        CYAN_ASSERT(m_pos + size <= m_size, "Write to UniformBuffer out of bound")
        memcpy((u8*)m_data + m_pos, data, size);
        m_pos += size;
    }

    void write(u32 data)
    {
        CYAN_ASSERT(m_pos + 4 <= m_size, "Write from UniformBuffer out of bound")
        u32* ptr = (u32*)((u8*)m_data + m_pos);
        *ptr = data;
        m_pos += 4;
    }
    
    void write(f32 data)
    {
        CYAN_ASSERT(m_pos + 4 <= m_size, "Write from UniformBuffer out of bound")
        f32* ptr = (f32*)((u8*)m_data + m_pos);
        *ptr = data;
        m_pos += 4;
    }
};