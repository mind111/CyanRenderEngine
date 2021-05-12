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