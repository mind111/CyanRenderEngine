#pragma once
namespace Cyan
{
    class GHTexture2D;
    class GfxStaticSubMesh;

    class RenderingUtils
    {
    public:
        ~RenderingUtils();

        static RenderingUtils* get();
        void initialize();
        void deinitialize();

        // static void renderScreenPass();

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
