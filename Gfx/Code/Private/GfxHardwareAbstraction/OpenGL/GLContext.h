#pragma once

#include "GfxHardwareContext.h"
#include "GLStaticMesh.h"

namespace Cyan
{
    class GLGHContext : public GfxHardwareContext
    {
    public:
        GLGHContext();
        virtual ~GLGHContext();

        virtual void initialize() override;
        virtual void deinitialize() override;

        // buffers
        virtual GHVertexBuffer* createVertexBuffer() override;
        virtual GHIndexBuffer* createIndexBuffer() override;
        // shaders
        virtual GHShader* createVertexShader(const std::string& text) override;
        virtual GHShader* createPixelShader(const std::string& text) override;
        virtual GHShader* createComputeShader(const std::string& text) override;
        virtual GHGfxPipeline* createGfxPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> ps) override;
        // framebuffers
        virtual GHFramebuffer* createFramebuffer(u32 width, u32 height) override;
        virtual void setViewport(i32 x, i32 y, i32 width, i32 height) override;
        // textures
        virtual std::unique_ptr<GHDepthTexture> createDepthTexture(const GHDepthTexture::Desc& desc) override;
        virtual std::unique_ptr<GHTexture2D> createTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D) override;
        // static mesh
        virtual std::unique_ptr<GfxStaticSubMesh> createGfxStaticSubMesh(Geometry* geometry) override;

        virtual void enableDepthTest() override;
        virtual void disableDepthTest() override;

        // debugging
        virtual void pushGpuDebugMarker(const char* markerName) override;
        virtual void popGpuDebugMarker() override;
    private:
    };
}