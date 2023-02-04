#pragma once
#include <string>
#include <vector>

#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"

namespace Cyan
{
    struct VertexBuffer : public GpuResource
    {
        struct Attribute
        {
            enum class Type
            {
                kVec2 = 0,
                kVec3,
                kVec4
            };

            std::string name;
            Type type;
            u32 offset;
        };

        struct Spec
        {
            void addVertexAttribute(const char* attribName, VertexBuffer::Attribute::Type attribType);

            u32 stride = 0;
            std::vector<Attribute> attributes;
        };

        VertexBuffer() = delete;
        VertexBuffer(const Spec& inSpec, void* inData, u32 sizeInBytes);
        ~VertexBuffer() { }

        Spec spec;
    };
}