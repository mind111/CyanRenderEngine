#include "Core.h"
#include "GLContext.h"
#include "GLBuffer.h"
#include "GLShader.h"
#include "GfxHardwareAbstraction/GHInterface/GHPipeline.h"
#include "GfxHardwareAbstraction/OpenGL/GLPipeline.h"
#include "GLFramebuffer.h"
#include "GLTexture.h"

namespace Cyan
{
    GLGHContext::GLGHContext()
    {

    }

    GLGHContext::~GLGHContext()
    {

    }

    void GLGHContext::initialize()
    {

    }

    void GLGHContext::deinitialize()
    {

    }

    GHVertexBuffer* GLGHContext::createVertexBuffer()
    {
        return nullptr;
    }

    GHIndexBuffer* GLGHContext::createIndexBuffer()
    {
        return nullptr;
    }

    GHShader* GLGHContext::createVertexShader(const std::string& text)
    {
        return new GLVertexShader(text);
    }

    GHShader* GLGHContext::createPixelShader(const std::string& text)
    {
        return new GLPixelShader(text);
    }

    GHShader* GLGHContext::createComputeShader(const std::string& text)
    {
        return new GLComputeShader(text);
    }

    Cyan::GHShader* GLGHContext::createGeometryShader(const std::string& text)
    {
        return new GLGeometryShader(text);
    }

    GHGfxPipeline* GLGHContext::createGfxPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> ps)
    {
        auto glVS = std::dynamic_pointer_cast<GLVertexShader>(vs);
        auto glPS = std::dynamic_pointer_cast<GLPixelShader>(ps);
        if (glVS != nullptr && glPS != nullptr)
        {
            return new GLGfxPipeline(glVS, glPS);
        }
        assert(0); // catching errors here
        return nullptr;
    }

    GHComputePipeline* GLGHContext::createComputePipeline(std::shared_ptr<GHShader> cs)
    {
        auto glCS = std::dynamic_pointer_cast<GLComputeShader>(cs);
        if (glCS != nullptr)
        {
            return new GLComputePipeline(glCS);
        }
        assert(0); // catching errors here
        return nullptr;
    }

    GHGeometryPipeline* GLGHContext::createGeometryPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> gs, std::shared_ptr<GHShader> ps)
    {
        auto glVS = std::dynamic_pointer_cast<GLVertexShader>(vs);
        auto glGS = std::dynamic_pointer_cast<GLGeometryShader>(gs);
        auto glPS = std::dynamic_pointer_cast<GLPixelShader>(ps);
        if (glVS != nullptr && glGS != nullptr && glPS != nullptr)
        {
            return new GLGeometryPipeline(glVS, glGS, glPS);
        }
        assert(0);
        return nullptr;
    }

    GHFramebuffer* GLGHContext::createFramebuffer(u32 width, u32 height)
    {
        return new GLFramebuffer(width, height);
    }

    void GLGHContext::setViewport(i32 x, i32 y, i32 width, i32 height)
    {
        glViewport(x, y, width, height);
    }

    std::unique_ptr<GHDepthTexture> GLGHContext::createDepthTexture(const GHDepthTexture::Desc& desc)
    {
        return std::move(std::unique_ptr<GHDepthTexture>(new GLDepthTexture(desc)));
    }

    std::unique_ptr<GHTexture2D> GLGHContext::createTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D)
    {
        return std::move(std::unique_ptr<GHTexture2D>(new GLTexture2D(desc, sampler2D)));
    }

    std::unique_ptr<Cyan::GHTextureCube> GLGHContext::createTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube /*= GHSamplerCube()*/)
    {
        return std::move(std::unique_ptr<GHTextureCube>(new GLTextureCube(desc, samplerCube)));
    }

    std::unique_ptr<Cyan::GHTexture3D> GLGHContext::createTexture3D(const GHTexture3D::Desc& desc, const GHSampler3D& sampler3D /*= GHSampler3D()*/)
    {
        return std::move(std::unique_ptr<GHTexture3D>(new GLTexture3D(desc, sampler3D)));
    }

    std::unique_ptr<GHRWBuffer> GLGHContext::createRWBuffer(u32 sizeInBytes)
    {
        return std::move(std::make_unique<GLShaderStorageBuffer>(sizeInBytes));
    }

    std::unique_ptr<Cyan::GHAtomicCounterBuffer> GLGHContext::createAtomicCounterBuffer()
    {
        return std::move(std::make_unique<GLAtomicCounterBuffer>());
    }

    std::unique_ptr<GfxStaticSubMesh> GLGHContext::createGfxStaticSubMesh(Geometry* geometry)
    {
        return std::move(std::unique_ptr<GLStaticSubMesh>(new GLStaticSubMesh(geometry)));
    }

    void GLGHContext::setGeometryMode(const GeometryMode& mode)
    {
        switch (mode)
        {
        case GeometryMode::kTriangles: m_geometryMode = GL_TRIANGLES; break;
        case GeometryMode::kLines: m_geometryMode = GL_LINES; break;
        case GeometryMode::kPoints: m_geometryMode = GL_POINTS; break;
        default:
            break;
        }
    }

    void GLGHContext::drawArrays(u32 numVertices)
    {
        glDrawArrays(m_geometryMode, 0, numVertices);
    }

    void GLGHContext::drawIndices(u32 numIndices)
    {
        glDrawElements(m_geometryMode, numIndices, GL_UNSIGNED_INT, 0);
    }

    void GLGHContext::dispatchCompute(i32 threadGroupSizeX, i32 threadGroupSizeY, i32 threadGroupSizeZ)
    {
        glDispatchCompute(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
    }

    void GLGHContext::enableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void GLGHContext::disableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
    }

    void GLGHContext::enableBackfaceCulling()
    {
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
    }

    void GLGHContext::disableBackfaceCulling()
    {
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
    }

    void GLGHContext::enableBlending()
    {
        glEnable(GL_BLEND);
    }

    void GLGHContext::disableBlending()
    {
        glDisable(GL_BLEND);
    }

    void GLGHContext::setBlendingMode(const BlendingMode& bm)
    {
        switch (bm)
        {
        case BlendingMode::kAdditive: {
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        }
        default:
            assert(0);
            break;
        }
    }

    void GLGHContext::pushGpuDebugMarker(const char* markerName)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, markerName);
    }

    void GLGHContext::popGpuDebugMarker()
    {
        glPopDebugGroup();
    }
}