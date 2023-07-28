#include "GLStaticMesh.h"
#include "GLBuffer.h"
#include "Geometry.h"

namespace Cyan
{
    static GLStaticSubMesh::PrimitiveMode translate(const Geometry::Type& geomType)
    {
        switch (geomType)
        {
        case Geometry::Type::kTriangles: return GLStaticSubMesh::PrimitiveMode::kTriangles;
        case Geometry::Type::kQuads:
        case Geometry::Type::kPointCloud:
        case Geometry::Type::kLines:
        default: assert(0); return GLStaticSubMesh::PrimitiveMode::kInvalid;
        }
    }

    GLStaticSubMesh::GLStaticSubMesh(Geometry* geometry)
        : m_numVertices(geometry->numVertices())
        , m_numIndices(geometry->numIndices())
    {
        Geometry::Type geometryType = geometry->getGeometryType();
        m_primitiveMode = translate(geometryType);

        switch (geometry->getGeometryType())
        {
        case Geometry::Type::kTriangles: {
            auto triangles = static_cast<Triangles*>(geometry);
            u32 sizeInBytes = sizeof(triangles->vertices[0]) * static_cast<u32>(triangles->vertices.size());
            assert(sizeInBytes > 0);
            m_vb = std::make_unique<GLVertexBuffer>(geometry->getVertexSpec(), sizeInBytes, triangles->vertices.data());
            m_ib = std::make_unique<GLIndexBuffer>(triangles->indices);
            m_va = std::make_unique<GLVertexArray>(m_vb.get(), m_ib.get());
        } break;
        case Geometry::Type::kLines:
            // todo: implement this  
        case Geometry::Type::kPointCloud:
            // todo: implement this  
        case Geometry::Type::kQuads:
            // todo: implement this  
        default:
            assert(0);
        }
    }

    GLStaticSubMesh::~GLStaticSubMesh()
    {

    }

    void GLStaticSubMesh::draw()
    {
        m_va->bind();
        switch (m_primitiveMode)
        {
        case PrimitiveMode::kTriangles:
            glDrawElements(GL_TRIANGLES, numIndices(), GL_UNSIGNED_INT, 0);
            break;
        default:
            assert(0);
            break;
        }
    };
}