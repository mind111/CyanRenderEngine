#pragma once

#include "Core.h"
#include "GfxHardwareAbstraction/GHInterface/GHPipeline.h"
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

    class GLComputePipeline : public GLObject, public GHComputePipeline
    {
    public:
        GLComputePipeline(std::shared_ptr<GLComputeShader> cs);
        ~GLComputePipeline();

        virtual void bind() override;
        virtual void unbind() override;

    protected:
        std::shared_ptr<GLComputeShader> m_computeShader = nullptr;
    };

    class GLGeometryPipeline : public GLObject, public GHGeometryPipeline
    {
    public:
        GLGeometryPipeline(std::shared_ptr<GLVertexShader> vs, std::shared_ptr<GLGeometryShader> gs, std::shared_ptr<GLPixelShader> ps);
        ~GLGeometryPipeline();

        virtual void bind() override;
        virtual void unbind() override;

    protected:
        std::shared_ptr<GLVertexShader> m_vertexShader = nullptr;
        std::shared_ptr<GLGeometryShader> m_geometryShader = nullptr;
        std::shared_ptr<GLPixelShader> m_pixelShader = nullptr;
    };
}
