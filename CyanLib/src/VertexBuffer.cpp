#include <cassert>
#include "VertexBuffer.h"

namespace Cyan
{
    void VertexBuffer::Spec::addVertexAttribute(const char* attribName, VertexBuffer::Attribute::Type attribType)
    {
        Attribute attribute = { };
        attribute.m_name = std::string(attribName);
        attribute.type = attribType;
        attribute.offset = stride;

        switch (attribute.type)
        {
        case Attribute::Type::kVec2:
            stride += sizeof(f32) * 2;
            break;
        case Attribute::Type::kVec3:
            stride += sizeof(f32) * 3;
            break;
        case Attribute::Type::kVec4:
            stride += sizeof(f32) * 4;
            break;
        default:
            assert(0);
        }

        attributes.push_back(attribute);
    }

    VertexBuffer::VertexBuffer(const Spec& inSpec, void* data, u32 sizeInBytes)
        : GfxResource(), spec(inSpec)
    {
        glCreateBuffers(1, &glObject);
        glNamedBufferData(getGpuResource(), sizeInBytes, data, GL_STATIC_DRAW);
    }
}
