#pragma once

#include "Common.h"

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
    std::string m_name;
    void* m_valuePtr;
};