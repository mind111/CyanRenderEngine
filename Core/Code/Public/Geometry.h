#pragma once

#include <vector>

#include "MathLibrary.h"

namespace Cyan
{
    // todo: support for arbitrary type of vertex, no more hardcoded vertex spec
    struct VertexAttribute
    {
        friend struct VertexSpec;
        enum class Type
        {
            kPosition = 0,
            kNormal,
            kTangent,
            kTexCoord0,
            kTexCoord1,
            kColor,
            kInvalid
        };

        const Type& getType() const
        {
            return type;
        }

        const u32 getNumComponent() const 
        {
            switch (type)
            {
            case Type::kPosition: return 3u;
            case Type::kNormal: return 3u;
            case Type::kTangent: return 4u;
            case Type::kTexCoord0: return 2u;
            case Type::kTexCoord1: return 2u;
            case Type::kColor: return 4u;
            default: assert(0); return 0;
            }
        }

        const u32 getOffset() const
        {
            return offset;
        }

    private:

        VertexAttribute(const Type& inType)
            : type(inType)
        {
        }

        u32 getSizeInBytes()
        {
            switch (type)
            {
            case Type::kPosition: return sizeof(glm::vec3);
            case Type::kNormal: return sizeof(glm::vec3);
            case Type::kTangent: return sizeof(glm::vec4);
            case Type::kTexCoord0: return sizeof(glm::vec2);
            case Type::kTexCoord1: return sizeof(glm::vec2);
            case Type::kColor: return sizeof(glm::vec4);
            default: assert(0); return 0;
            }
        }

        Type type = Type::kInvalid;
        u32 offset = 0;
    };

    struct VertexSpec
    {
        const VertexAttribute& operator[](u32 index) const
        {
            assert(index < numAttributes());
            return attributes[index];
        }

        u32 numAttributes() const
        {
            return static_cast<u32>(attributes.size());
        }

        void addAttribute(const VertexAttribute::Type& attribType)
        {
            VertexAttribute attribute(attribType);
            attribute.offset = sizeInBytes;
            sizeInBytes += attribute.getSizeInBytes();
            attributes.push_back(attribute);
        }

        u32 getSizeInBytes() const { return sizeInBytes; }

    private:
        std::vector<VertexAttribute> attributes;
        u32 sizeInBytes = 0;
    };

    struct Geometry
    {
        enum class Type
        {
            kTriangles = 0,
            kQuads,
            kPointCloud,
            kLines,
        };

        virtual Type getGeometryType() = 0;
        virtual VertexSpec getVertexSpec() = 0;
        virtual u32 numVertices() = 0;
        virtual u32 numIndices() = 0;
    };

    // todo: this works 
    struct Triangles : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec4 tangent;
            glm::vec2 texCoord0;
            glm::vec2 texCoord1;
        };

        Triangles() = default;
        Triangles(const std::vector<Triangles::Vertex>& inVertices, const std::vector<u32>& inIndices)
            : vertices(inVertices), indices(inIndices)
        {

        }
        ~Triangles() { }

        virtual Type getGeometryType() override { return Type::kTriangles; }
        virtual VertexSpec getVertexSpec() override
        {
            VertexSpec outSpec;
            outSpec.addAttribute(VertexAttribute::Type::kPosition);
            outSpec.addAttribute(VertexAttribute::Type::kNormal);
            outSpec.addAttribute(VertexAttribute::Type::kTangent);
            outSpec.addAttribute(VertexAttribute::Type::kTexCoord0);
            outSpec.addAttribute(VertexAttribute::Type::kTexCoord1);
            return outSpec;
        }

        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct PointCloud : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
        };

        virtual Type getGeometryType() override { return Type::kPointCloud; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }
        virtual VertexSpec getVertexSpec() override
        {
            VertexSpec outSpec;
            outSpec.addAttribute(VertexAttribute::Type::kPosition);
            return outSpec;
        }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Quads : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 texCoord;
        };

        virtual Type getGeometryType() override { return Type::kQuads; }
        virtual VertexSpec getVertexSpec() override
        {
            VertexSpec outSpec;
            outSpec.addAttribute(VertexAttribute::Type::kPosition);
            outSpec.addAttribute(VertexAttribute::Type::kNormal);
            outSpec.addAttribute(VertexAttribute::Type::kTexCoord0);
            return outSpec;
        }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Lines : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec4 color;
        };

        Lines() = default;
        Lines(const std::vector<Vertex>& inVertices, const std::vector<u32>& inIndices)
            : vertices(inVertices)
            , indices(inIndices)
        {

        }

        virtual Type getGeometryType() override { return Type::kLines; }
        virtual VertexSpec getVertexSpec() override
        {
            VertexSpec outSpec;
            outSpec.addAttribute(VertexAttribute::Type::kPosition);
            outSpec.addAttribute(VertexAttribute::Type::kColor);
            return outSpec;
        }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };
}
