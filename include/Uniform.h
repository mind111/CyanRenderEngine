#pragma once

#include "Common.h"

const int kMaxUniformNameLen = 64;

// TODO: Uniform should no longer hold a pointer to the acutal data; should create UniformInstance class
//       to hold a handle to the referenced Uniforom and the actual data, UniformBuffer will thus be a vector
//       of UniformInstance; UniformInstance should guarantee that the memory allocated for the data is contiguous
//       with the uniform handle. | handle |            data              | ...

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
        u_samplerCube,
        u_undefined // Error
    };

    u32 getSize();

    Type m_type;
    char m_name[kMaxUniformNameLen];
    void* m_valuePtr;
};

// TODO: draw visualization of the buffer layout 
// TODO: implemenet auto-resizing
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
    void write(void* data, u32 size);
    void write(u32 data);
    void write(i32 data);
    void write(f32 data);
    void finish();
    void clear();
    void debugPrint();
};

struct NewUniform
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
        u_samplerCube,
        u_undefined // Error
    };
    char m_name[kMaxUniformNameLen];
    Type m_type;    
    u32 m_sizeInBytes;

    // UniformInstance* createInstance() {
    // u32 size = sizeof(UniformHandle) + m_sizeInBytes;
    // return reinterpret_cast<UniformInstance*>(CYAN_ALLOC(size);
    // };
};

struct UniformInstance 
{
    UniformHandle m_handle;
    char*         m_data;
};

struct UniformInstanceBuffer
{
    std::vector<UniformInstance> m_buffer; 
};