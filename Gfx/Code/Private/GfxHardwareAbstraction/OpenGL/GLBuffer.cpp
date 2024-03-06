#include <stack>

#include "Core.h"
#include "GLBuffer.h"

namespace Cyan
{
    GLVertexBuffer::GLVertexBuffer(const VertexSpec& vertexSpec, i32 sizeInBytes, const void* data)
        : GLObject()
        , m_vertexSpec(vertexSpec)
        , m_bufferSizeInBytes(sizeInBytes)
    {
        glCreateBuffers(1, &m_name);
        // todo: expose the usage parameter
        glNamedBufferData(m_name, m_bufferSizeInBytes, data, GL_STATIC_DRAW);
    }
    
    GLVertexBuffer::~GLVertexBuffer()
    {
        glDeleteBuffers(1, &m_name);
    }

    void GLVertexBuffer::bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_name);
    }

    void GLVertexBuffer::unbind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLVertexBuffer::update(u32 byteOffset, u32 numOfBytes, const void* data)
    {
        assert(byteOffset + numOfBytes <= m_bufferSizeInBytes);
        glNamedBufferSubData(getName(), byteOffset, numOfBytes, data);
    }

    GLIndexBuffer::GLIndexBuffer(const std::vector<u32>& indices)
    {
        glCreateBuffers(1, &m_name);
        u32 sizeInBytes = sizeof(u32) * static_cast<u32>(indices.size());
        m_bufferSizeInBytes = sizeInBytes;
        glNamedBufferData(m_name, m_bufferSizeInBytes, indices.data(), GL_STATIC_DRAW);
    }

    GLIndexBuffer::~GLIndexBuffer()
    {
        glDeleteBuffers(1, &m_name);
    }

    void GLIndexBuffer::bind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_name);
    }

    void GLIndexBuffer::unbind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    GLVertexArray::GLVertexArray(GLVertexBuffer* vb, GLIndexBuffer* ib)
        : m_vb(vb), m_ib(ib)
    {
        assert(vb != nullptr && ib != nullptr);
        glCreateVertexArrays(1, &m_name);

        const auto& vertexSpec = vb->getVertexSpec();
        glVertexArrayVertexBuffer(m_name, 0, vb->getName(), 0, vertexSpec.getSizeInBytes());
        bind();
        vb->bind();
        for (u32 i = 0; i < vertexSpec.numAttributes(); ++i)
        {
            const auto& attribute = vertexSpec[i];
            GLint size = 0;
            GLenum dataType;
            switch (attribute.getType())
            {
            case VertexAttribute::Type::kPosition: 
            case VertexAttribute::Type::kNormal:
                size = 3;
                dataType = GL_FLOAT;
                break;
            case VertexAttribute::Type::kTangent: 
                size = 4;
                dataType = GL_FLOAT;
                break;
            case VertexAttribute::Type::kTexCoord0: 
            case VertexAttribute::Type::kTexCoord1: 
                size = 2;
                dataType = GL_FLOAT;
                break;
            case VertexAttribute::Type::kColor: 
                size = 4;
                dataType = GL_FLOAT;
                break;
            default: 
                assert(0); 
                break;
            }

            glVertexAttribPointer(i, size, dataType, GL_FALSE, vertexSpec.getSizeInBytes(), reinterpret_cast<const void*>(attribute.getOffset()));
            glEnableVertexArrayAttrib(m_name, i);
        }
        vb->unbind();
        unbind();
        glVertexArrayElementBuffer(m_name, ib->getName());
    }

    GLVertexArray::~GLVertexArray()
    {

    }

    void GLVertexArray::bind()
    {
        glBindVertexArray(m_name);
    }

    void GLVertexArray::unbind()
    {
        glBindVertexArray(0);
    }

    GLShaderStorageBuffer::GLShaderStorageBuffer(u32 sizeInBytes)
        : GLObject()
        , m_sizeInBytes(sizeInBytes)
    {
        glCreateBuffers(1, &m_name);
        // todo: the usage hint should be configurable
        glNamedBufferData(m_name, sizeInBytes, nullptr, GL_DYNAMIC_COPY);
    }

    GLShaderStorageBuffer::~GLShaderStorageBuffer()
    {
        if (isBound())
        {
            unbind();
        }
        glDeleteBuffers(1, &m_name);
    }

    static constexpr i32 kNumShaderStorageBindings = 16;
    static std::stack<i32> s_freeShaderStorageUnits;
    static std::array<GLShaderStorageBuffer*, kNumShaderStorageBindings> s_shaderStorageBindings;

    static i32 allocShaderStorageUnit()
    {
        static bool bInitialized = false;
        if (!bInitialized)
        {
            for (i32 unit = kNumShaderStorageBindings - 1; unit >= 0; --unit)
            {
                s_shaderStorageBindings[unit] = nullptr;
                s_freeShaderStorageUnits.push(unit);
            }
            bInitialized = true;
        }

        if (!s_freeShaderStorageUnits.empty())
        {
            i32 unit = s_freeShaderStorageUnits.top();
            s_freeShaderStorageUnits.pop();
            assert(s_shaderStorageBindings[unit] == nullptr);
            return unit;
        }
        else
        {
            assert(0); // unreachable
            return -1;
        }
    }

    static void releaseShaderStorageUnit(i32 shaderStorageUnit)
    {
        auto boundBuffer = s_shaderStorageBindings[shaderStorageUnit];
        assert(boundBuffer != nullptr);
        s_shaderStorageBindings[shaderStorageUnit] = nullptr;
        s_freeShaderStorageUnits.push(shaderStorageUnit);
    }

    void GLShaderStorageBuffer::bind()
    {
        if (!isBound())
        {
            i32 shaderStorageUnit = allocShaderStorageUnit();
            if (shaderStorageUnit >= 0)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderStorageUnit, getName());
                m_boundUnit = shaderStorageUnit;
                s_shaderStorageBindings[shaderStorageUnit] = this;
            }
        }
    }

    void GLShaderStorageBuffer::unbind()
    {
        if (isBound())
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_boundUnit, 0);
            releaseShaderStorageUnit(m_boundUnit);
            m_boundUnit = INVALID_SHADER_STORAGE_UNIT;
        }
    }

    void GLShaderStorageBuffer::read(void* dst, u32 dstOffset, u32 srcOffset, u32 bytesToRead)
    {
        assert(dst != nullptr);
        assert((srcOffset + bytesToRead) <= m_sizeInBytes);
        glGetNamedBufferSubData(getName(), srcOffset, bytesToRead, (u8*)dst + dstOffset);
    }

    void GLShaderStorageBuffer::write(void* src, u32 srcOffset, u32 dstOffset, u32 bytesToWrite)
    {
        assert(src != nullptr);
        assert((dstOffset + bytesToWrite) <= m_sizeInBytes);
        glNamedBufferSubData(getName(), dstOffset, bytesToWrite, (u8*)src + srcOffset);
    }

    GLAtomicCounterBuffer::GLAtomicCounterBuffer()
    {
        glCreateBuffers(1, &m_name);
        // todo: the usage hint should be configurable
        glNamedBufferData(m_name, sizeof(u32), nullptr, GL_DYNAMIC_COPY);
    }

#if 0
    static constexpr i32 kNumAtomicCounterBufferBindings = 8;
    static std::stack<i32> s_freeAtomicCounterBindingUnits;
    static std::array<GLAtomicCounterBuffer*, kNumAtomicCounterBufferBindings> s_atomicCounterBufferBindings;

    static GLuint allocateAtomicCounterBindingUnit()
    {
        static bool bInitialized = false;
        if (!bInitialized)
        {
            for (i32 unit = kNumAtomicCounterBufferBindings - 1; unit >= 0; --unit)
            {
                s_atomicCounterBufferBindings[unit] = nullptr;
                s_freeAtomicCounterBindingUnits.push(unit);
            }
            bInitialized = true;
        }

        if (!s_freeAtomicCounterBindingUnits.empty())
        {
            i32 unit = s_freeAtomicCounterBindingUnits.top();
            s_freeAtomicCounterBindingUnits.pop();
            assert(s_atomicCounterBufferBindings[unit] == nullptr);
            return unit;
        }
        else
        {
            assert(0); // unreachable
            return -1;
        }
    }

    static void releaseAtomicCounterBindingUnit(GLuint unit)
    {
        auto boundCounter = s_atomicCounterBufferBindings[unit];
        assert(boundCounter != nullptr);
        s_atomicCounterBufferBindings[unit] = nullptr;
        s_freeAtomicCounterBindingUnits.push(unit);
    }
#endif

    GLAtomicCounterBuffer::~GLAtomicCounterBuffer()
    {
        if (isBound())
        {
            unbind();
        }

        glDeleteBuffers(1, &m_name);
    }

    void GLAtomicCounterBuffer::bind(u32 bindingUnit)
    {
        if (m_boundUnit != bindingUnit)
        {
            glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, bindingUnit, getName());
            m_boundUnit = bindingUnit;
        }
    }

    void GLAtomicCounterBuffer::unbind()
    {
        if (isBound())
        {
            glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, m_boundUnit, 0);
            m_boundUnit = INVALID_ATOMIC_COUNTER_UNIT;
        }
    }

    void GLAtomicCounterBuffer::read(u32* dst)
    {
        glGetNamedBufferSubData(m_name, 0, sizeof(u32), dst);
    }

    void GLAtomicCounterBuffer::write(u32 data)
    {
        glNamedBufferSubData(m_name, 0, sizeof(u32), &data);
    }
}