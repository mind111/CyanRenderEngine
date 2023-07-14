#include "GLContext.h"
#include "GLBuffer.h"
#include "GLShader.h"
#include "GLPipeline.h"

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

    GHVertexShader* GLGHContext::createVertexShader(const char* text)
    {
        return new GLVertexShader(text);
    }

    GHPixelShader* GLGHContext::createPixelShader(const char* text)
    {
        return new GLPixelShader(text);
    }

    GHComputeShader* GLGHContext::createComputeShader(const char* text)
    {
        return new GLComputeShader(text);
    }

    GHGfxPipeline* GLGHContext::createGfxPipeline(const char* vsText, const char* psText)
    {
        auto vs = std::make_shared<GLVertexShader>(vsText);
        auto ps = std::make_shared<GLPixelShader>(psText);
        return new GLGfxPipeline(vs, ps);
    }
}
