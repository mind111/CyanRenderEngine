#include "Uniform.h"

u32 Uniform::getSize()
{
    u32 size;
    switch(m_type)
    {
        case u_float:
            size = 4;
            break;
        case u_int:
            size = 4;
            break;
        case u_vec3:
            size = 4 * 3;
            break;
        case u_vec4:
            size = 4 * 4;
            break;
        case u_mat4:
            size = 4 * 4 * 4;
            break;
        case u_sampler2D:
            size = 4;
        case u_samplerCube:
            size = 4;
        default:
            break;
    }
    return size;
}