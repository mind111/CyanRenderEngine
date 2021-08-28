#include "CyanAPI.h"
#include "Uniform.h"

float quadVertices[24] = {
    -1.f,  1.f,
    -1.f, -1.f,
     1.f, -1.f,
    -1.f,  1.f,
     1.f, -1.f,
     1.f,  1.f
};

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

void UniformBuffer::reset(u32 offset)
{
    m_pos = offset;
}

i32 UniformBuffer::readI32()
{
    i32 value = *(i32*)((u8*)m_data + m_pos);
    m_pos += sizeof(i32);
    return value;
}

f32 UniformBuffer::readF32()
{
    f32 value = *(f32*)((u8*)m_data + m_pos);
    m_pos += sizeof(f32);
    return value;
}

u32 UniformBuffer::readU32()
{
    u32 value = *(u32*)((u8*)m_data + m_pos);
    m_pos += sizeof(u32);
    return value;
}

// Read float3
glm::vec3 UniformBuffer::readVec3()
{
    float x = readF32();
    float y = readF32();
    float z = readF32();
    return glm::vec3(x, y, z);
}

glm::vec4 UniformBuffer::readVec4()
{
    float x = readF32();
    float y = readF32();
    float z = readF32();
    float w = readF32();
    return glm::vec4(x, y, z, w);
}

void* UniformBuffer::read(u32 size)
{
    CYAN_ASSERT(m_pos + size <= m_size, "Read from UniformBuffer out of bound")
    void* data = (void*)((u8*)m_data + m_pos);
    m_pos += size;
    return data;
}

void UniformBuffer::write(void* data, u32 size)
{
    CYAN_ASSERT(m_pos + size <= m_size, "Write to UniformBuffer out of bound")
    memcpy((u8*)m_data + m_pos, data, size);
    m_pos += size;
}

void UniformBuffer::write(u32 data)
{
    CYAN_ASSERT(m_pos + 4 <= m_size, "Write from UniformBuffer out of bound")
    u32* ptr = (u32*)((u8*)m_data + m_pos);
    *ptr = data;
    m_pos += 4;
}

void UniformBuffer::write(f32 data)
{
    CYAN_ASSERT(m_pos + 4 <= m_size, "Write from UniformBuffer out of bound")
    f32* ptr = (f32*)((u8*)m_data + m_pos);
    *ptr = data;
    m_pos += 4;
}

void UniformBuffer::write(i32 data)
{
    CYAN_ASSERT(m_pos + 4 <= m_size, "Write from UniformBuffer out of bound")
    i32* ptr = (i32*)((u8*)m_data + m_pos);
    *ptr = data;
    m_pos += 4;
}

void UniformBuffer::clear()
{
    memset(m_data, 0x0, m_size);
}

void UniformBuffer::finish()
{
    write(UniformHandle(-1));
    m_pos = 0;
}

void UniformBuffer::debugPrint()
{
    u32 cachedCurrentPos = m_pos;
    reset();
    printf("Buffer size: %u\n", m_size);
    for (;;)
    {
        UniformHandle handle = readU32();
        BREAK_WHEN(handle == UniformBuffer::End)
        Uniform* uniform = Cyan::getUniform(handle);
        switch (uniform->m_type)
        {
            case Uniform::Type::u_float:
            {
                f32 data = readF32();
                u32 offset = m_pos - sizeof(f32);
                printf("%u bytes| %u %s: %.2f \n", offset, handle, uniform->m_name, data);
                break;
            }
            case Uniform::Type::u_int:
            {
                i32 data = readI32();
                u32 offset = m_pos - sizeof(i32);
                printf("%u bytes| %u %s: %d \n", offset, handle, uniform->m_name, data);
                break;
            }
            case Uniform::Type::u_uint:
            {
                break;
            }
            case Uniform::Type::u_vec3:
            {
                glm::vec3 data = readVec3();
                u32 offset = m_pos - sizeof(f32) * 3;
                printf("%u bytes| %u %s: x: %.2f y: %.2f z: %.2f \n", offset, handle, uniform->m_name, data.x, data.y, data.z);
                break;
            }
            case Uniform::Type::u_vec4:
            {
                glm::vec4 data = readVec4();
                u32 offset = m_pos - sizeof(f32) * 4;
                printf("%u bytes| %u %s: x: %.2f y: %.2f z: %.2f, w: %.2f \n", offset, handle, uniform->m_name, data.x, data.y, data.z, data.w);
                break;
            }
            case Uniform::Type::u_mat4:
            {
                break;
            }
            default:
                break;
        }
    }
    reset(cachedCurrentPos);
}