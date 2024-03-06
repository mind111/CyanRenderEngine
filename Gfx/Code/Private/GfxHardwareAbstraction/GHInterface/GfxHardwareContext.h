#pragma once

#include "GHBuffer.h"
#include "GHTexture.h"
#include "GHShader.h"
#include "GHPipeline.h"
#include "GHFramebuffer.h"
#include "Geometry.h"
#include "GfxStaticMesh.h"

namespace Cyan
{
    enum GfxHardwareAPI
    {
        OpenGL = 0,
        Vulkan,
        Count
    };

    enum class GeometryMode
    {
        kTriangles = 0,
        kDefault = 0,
        kLines,
        kPoints,
        kCount
    };

    enum class BlendingMode
    {
        kAdditive = 0,
        kCount
    };

    static GfxHardwareAPI getActiveGfxHardwareAPI();

    class GfxHardwareContext
    {
    public:
        static GfxHardwareContext* create();
        static GfxHardwareContext* get();

        virtual void initialize() = 0;
        virtual void deinitialize() = 0;

        virtual ~GfxHardwareContext() { };
        // buffers
        virtual GHVertexBuffer* createVertexBuffer() = 0;
        virtual GHIndexBuffer* createIndexBuffer() = 0;
        // shaders
        virtual GHShader* createVertexShader(const std::string& text) = 0;
        virtual GHShader* createPixelShader(const std::string& text) = 0;
        virtual GHShader* createComputeShader(const std::string& text) = 0;
        virtual GHShader* createGeometryShader(const std::string& text) = 0;
        virtual GHGfxPipeline* createGfxPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> ps) = 0;
        virtual GHComputePipeline* createComputePipeline(std::shared_ptr<GHShader> cs) = 0;
        virtual GHGeometryPipeline* createGeometryPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> gs, std::shared_ptr<GHShader> ps) = 0;
        // framebuffers
        virtual GHFramebuffer* createFramebuffer(u32 width, u32 height) = 0;
        virtual void setViewport(i32 x, i32 y, i32 width, i32 height) = 0;
        // textures
        virtual std::unique_ptr<GHDepthTexture> createDepthTexture(const GHDepthTexture::Desc& desc) = 0;
        virtual std::unique_ptr<GHTexture2D> createTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D = GHSampler2D()) = 0;
        virtual std::unique_ptr<GHTexture3D> createTexture3D(const GHTexture3D::Desc& desc, const GHSampler3D& sampler3D = GHSampler3D()) = 0;
        virtual std::unique_ptr<GHTextureCube> createTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube = GHSamplerCube()) = 0;
        virtual std::unique_ptr<GHRWBuffer> createRWBuffer(u32 sizeInBytes) = 0;
        virtual std::unique_ptr<GHAtomicCounterBuffer> createAtomicCounterBuffer() = 0;
        // static mesh
        virtual std::unique_ptr<GfxStaticSubMesh> createGfxStaticSubMesh(Geometry* geometry) = 0;
        // draw
        virtual void setGeometryMode(const GeometryMode& mode) = 0;
        virtual void drawArrays(u32 numVertices) = 0;
        virtual void drawIndices(u32 numIndices) = 0;
        // compute
        virtual void dispatchCompute(i32 x, i32 y, i32 z) = 0;
        // pipeline misc state
        virtual void enableDepthTest() = 0;
        virtual void disableDepthTest() = 0;
        virtual void enableBackfaceCulling() = 0;
        virtual void disableBackfaceCulling() = 0;
        virtual void enableBlending() = 0;
        virtual void disableBlending() = 0;
        virtual void setBlendingMode(const BlendingMode& bm) = 0;

        // debugging
        virtual void pushGpuDebugMarker(const char* markerName) = 0;
        virtual void popGpuDebugMarker() = 0;
    protected:
        GfxHardwareContext() { }
    private:
        static GfxHardwareContext* s_instance;
    };
}