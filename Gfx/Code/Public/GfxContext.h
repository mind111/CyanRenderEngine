#pragma once

#include "Shader.h"
#include "GfxHardwareAbstraction/GHInterface/GHFramebuffer.h"
#include "GfxHardwareAbstraction/GHInterface/GfxHardwareContext.h"

namespace Cyan
{
    struct Viewport
    {
        i32 x = 0, y = 0;
        i32 width = 0, height = 0;
    };

    struct Geometry;

    class GfxContext
    {
    public:
        ~GfxContext();
        static GfxContext* get();

        void initialize();
        void deinitialize();

        void setFramebuffer(GHFramebuffer* framebuffer);
        void unsetFramebuffer();
        void setViewport(i32 x, i32 y, i32 width, i32 height);
        void unsetViewport();
        void enableDepthTest();
        void disableDepthTest();
        /**/
        void pushGpuDebugMarker(const char* markerName);
        void popGpuDebugMarker();

        void setGeometryMode(const GeometryMode& mode);
        void drawArrays(u32 numVertices);

        void dispatchCompute(ComputePipeline* cp, i32 threadGroupSizeX, i32 threadGroupSizeY, i32 threadGroupSizeZ);

        std::unique_ptr<GfxStaticSubMesh> createGfxStaticSubMesh(Geometry* geometry);

    private:
        GfxContext();

        GfxHardwareContext* m_GHCtx = nullptr;
        GfxPipeline* m_gfxPipeline = nullptr;
        GHFramebuffer* m_framebuffer = nullptr;
        Viewport m_viewport = { };

        bool bDepthTest = false;

        static GfxContext* s_instance;
    };

    struct GFX_API GpuDebugMarker
    {
        GpuDebugMarker(const char* markerName)
        {
            GfxContext::get()->pushGpuDebugMarker(markerName);
        }

        ~GpuDebugMarker()
        {
            GfxContext::get()->popGpuDebugMarker();
        }
    };
#define GPU_DEBUG_SCOPE(markerVar, markerName) GpuDebugMarker markerVar(markerName);
}