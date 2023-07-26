#include "Core.h"
#include "GLContext.h"
#include "GLBuffer.h"
#include "GLShader.h"
#include "GLPipeline.h"
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

    GHVertexBuffer* GLGHContext::createVertexBuffer()
    {
        return nullptr;
    }

    GHIndexBuffer* GLGHContext::createIndexBuffer()
    {
        return nullptr;
    }

    GHShader* GLGHContext::createVertexShader(const char* text)
    {
        return new GLVertexShader(text);
    }

    GHShader* GLGHContext::createPixelShader(const char* text)
    {
        return new GLPixelShader(text);
    }

    GHShader* GLGHContext::createComputeShader(const char* text)
    {
        return new GLComputeShader(text);
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

    std::unique_ptr<GHTexture2D> GLGHContext::createTexture2D(const GHTexture2D::Desc& desc)
    {
        return std::move(std::unique_ptr<GHTexture2D>(new GLTexture2D(desc)));
    }

    GLStaticSubMesh* GLGHContext::createGfxStaticSubMesh(Geometry* geometry)
    {
        return new GLStaticSubMesh(geometry);
    }
}
