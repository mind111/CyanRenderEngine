#pragma once

#include "Core.h"
#include "GHPipeline.h"
#include "GLObject.h"
#include "GLShader.h"

namespace Cyan
{
    class GLGfxPipeline : public GLObject, public GHGfxPipeline
    {
    public:
        GLGfxPipeline(std::shared_ptr<GLVertexShader> vs, std::shared_ptr<GLPixelShader> ps);
        ~GLGfxPipeline();

        virtual void bind() override;
        virtual void unbind() override;

    protected:
        std::shared_ptr<GLVertexShader> m_vertexShader = nullptr;
        std::shared_ptr<GLPixelShader> m_pixelShader = nullptr;
    };
}
