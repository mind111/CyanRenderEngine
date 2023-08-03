#pragma once

#include "Shader.h"
#include "RenderPass.h"

namespace Cyan
{
    class GHTexture2D;
    class GfxStaticSubMesh;

    using RenderTargetSetupFunc = const std::function<void(RenderPass&)>;
    using ShaderSetupFunc = const std::function<void(GfxPipeline* p)>;

    class RenderingUtils
    {
    public:
        ~RenderingUtils();

        static RenderingUtils* get();
        void initialize();
        void deinitialize();

        GfxStaticSubMesh* getUnitQuadMesh() { return s_unitQuadMesh; }
        GfxStaticSubMesh* getUnitCubeMesh() { return s_unitCubeMesh; }

        static void renderScreenPass(const glm::uvec2& renderResolution, const RenderTargetSetupFunc& renderTargetSetupFunc, GfxPipeline* p, ShaderSetupFunc& shaderSetupFunc);

        /**
         * Blit a 2D texture to the default framebuffer
         */
        static void renderToBackBuffer(GHTexture2D* srcTexture);
    private:
        RenderingUtils();

        static GfxStaticSubMesh* s_unitQuadMesh;
        static GfxStaticSubMesh* s_unitCubeMesh;
        static RenderingUtils* s_instance;
    };
}
