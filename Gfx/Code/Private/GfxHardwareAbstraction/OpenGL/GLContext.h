#pragma once

#include "glew/glew.h"
#include "GfxHardwareAbstraction/GHInterface/GfxHardwareContext.h"
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
        virtual GHComputePipeline* createComputePipeline(std::shared_ptr<GHShader> cs) override;
        // framebuffers
        virtual GHFramebuffer* createFramebuffer(u32 width, u32 height) override;
        virtual void setViewport(i32 x, i32 y, i32 width, i32 height) override;
        // textures
        virtual std::unique_ptr<GHDepthTexture> createDepthTexture(const GHDepthTexture::Desc& desc) override;
        virtual std::unique_ptr<GHTexture2D> createTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D) override;
        virtual std::unique_ptr<GHTextureCube> createTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube = GHSamplerCube()) override;
        virtual std::unique_ptr<GHRWBuffer> createRWBuffer(u32 sizeInBytes) override;
        // static mesh
        virtual std::unique_ptr<GfxStaticSubMesh> createGfxStaticSubMesh(Geometry* geometry) override;
        virtual void setGeometryMode(const GeometryMode& mode) override;
        // draw
        virtual void drawArrays(u32 numVertices) override;
        virtual void drawIndices(u32 numIndices) override;
        virtual void dispatchCompute(i32 x, i32 y, i32 z) override;

        virtual void enableDepthTest() override;
        virtual void disableDepthTest() override;
        virtual void enableBlending() override;
        virtual void disableBlending() override;
        virtual void setBlendingMode(const BlendingMode& bm) override;

        // debugging
        virtual void pushGpuDebugMarker(const char* markerName) override;
        virtual void popGpuDebugMarker() override;
    private:
        GLenum m_geometryMode = GL_TRIANGLES;
    };
}