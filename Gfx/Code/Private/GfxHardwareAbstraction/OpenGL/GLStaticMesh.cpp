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
        case Geometry::Type::kPointCloud: return GLStaticSubMesh::PrimitiveMode::kPoints;
        case Geometry::Type::kLines: return GLStaticSubMesh::PrimitiveMode::kLines;
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
        case Geometry::Type::kLines: {
            auto lines = static_cast<Lines*>(geometry);
            u32 sizeInBytes = sizeof(lines->vertices[0]) * static_cast<u32>(lines->vertices.size());
            assert(sizeInBytes > 0);
            m_vb = std::make_unique<GLVertexBuffer>(geometry->getVertexSpec(), sizeInBytes, lines->vertices.data());
            m_ib = std::make_unique<GLIndexBuffer>(lines->indices);
            m_va = std::make_unique<GLVertexArray>(m_vb.get(), m_ib.get());
        } break;
        case Geometry::Type::kPointCloud: {
            auto points = static_cast<PointCloud*>(geometry);
            u32 sizeInBytes = sizeof(points->vertices[0]) * static_cast<u32>(points->vertices.size());
            assert(sizeInBytes > 0);
            m_vb = std::make_unique<GLVertexBuffer>(geometry->getVertexSpec(), sizeInBytes, points->vertices.data());
            m_ib = std::make_unique<GLIndexBuffer>(points->indices);
            m_va = std::make_unique<GLVertexArray>(m_vb.get(), m_ib.get());
            // todo: implement this  
        } break;
        case Geometry::Type::kQuads:
            // todo: implement this  
        default:
            assert(0);
        }
    }

    GLStaticSubMesh::~GLStaticSubMesh()
    {

    }

    void GLStaticSubMesh::updateVertices(u32 byteOffset, u32 numOfBytes, const void* data)
    {
        m_vb->update(byteOffset, numOfBytes, data);
    }

    void GLStaticSubMesh::draw()
    {
        m_va->bind();
        switch (m_primitiveMode)
        {
        case PrimitiveMode::kTriangles:
            glDrawElements(GL_TRIANGLES, numIndices(), GL_UNSIGNED_INT, 0);
            break;
        case PrimitiveMode::kLines:
            glDrawElements(GL_LINES, numIndices(), GL_UNSIGNED_INT, 0);
            break;
        case PrimitiveMode::kPoints:
            glDrawElements(GL_POINTS, numIndices(), GL_UNSIGNED_INT, 0);
            break;
        default:
            assert(0);
            break;
        }
    };
}