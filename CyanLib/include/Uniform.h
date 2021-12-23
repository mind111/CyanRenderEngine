#pragma once

#include "Common.h"

const int kMaxUniformNameLen = 64;

struct Uniform
{
    enum Type
    {
        u_float = 0,
        u_int,
        u_uint,
        u_vec3,
        u_vec4,
        u_mat4,
        u_sampler2D,
        u_sampler2DArray,
        u_samplerCube,
        u_sampler3D,
        u_sampler2DShadow,
        u_image3D,
        u_uimage3D,
        u_atomic_uint,
        u_undefined // Error
    };
    u32 getSize();

    Type m_type;
    char m_name[kMaxUniformNameLen];
    void* m_valuePtr;
};

// TODO: implemenet auto-resizing
// TODO: This should really be a ByteBuffer, UniformBuffer should be a wrapper 
// of ByteBuffer
struct UniformBuffer
{
    enum
    {
        End = UniformHandle(-1)
    };

    u32 m_size;
    u32 m_pos;
    void* m_data;

    void reset(u32 offset=0);
    f32 readF32();
    u32 readU32();
    i32 readI32();
    glm::vec3 readVec3();
    glm::vec4 readVec4();
    void* read(u32 size);
    void write(const void* data, u32 size);
    void write(u32 data);
    void write(i32 data);
    void write(f32 data);
    void finish();
    void clear();
    void debugPrint();
};