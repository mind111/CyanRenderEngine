#pragma once

#include "scene.h"
#include "Shader.h"
#include "GfxContext.h"
#include "RenderTarget.h"
#include "Geometry.h"

namespace Cyan
{
    void onRendererInitialized(glm::vec2 renderSize);

    struct RenderPass
    {
        RenderTarget* m_renderTarget;
        Viewport m_viewport;
        RenderPass(RenderTarget* renderTarget, Viewport viewport)
            : m_renderTarget(renderTarget), m_viewport(viewport)
        {

        }

        static void onInit() { }
        virtual void render() = 0;
    };

    struct ScenePass : public RenderPass
    {
        ScenePass(RenderTarget* dstRenderTarget, Viewport viewport, Scene* scene);
        ~ScenePass();

        static void onInit();
        virtual void render() override;

        Scene* m_scene;
    };

    struct BloomPassInputs
    {
        // TODO: output should really just be a texture
        Texture* sceneTexture;
        Texture* bloomOutputTexture;
    };

    struct BloomPass : public RenderPass
    {
        BloomPass(RenderTarget* rt, Viewport vp, BloomPassInputs inputs); 
        ~BloomPass() { }

        static void onInit(u32 windowWidth, u32 windowHeight);
        virtual void render() override;
        static Texture* getBloomOutput();
        void bloomDownSample();
        void bloomUpscale();

        struct BloomSurface
        {
            RenderTarget* m_renderTarget;
            Texture* m_pingPongColorBuffers[2];
        };

        static const u32 kNumBloomDsPass = 6u;
        static Shader* s_bloomSetupShader;
        static MaterialInstance* s_bloomSetupMatl;
        static RenderTarget* s_bloomSetupRT;                         // preprocess surface
        static BloomSurface s_bloomDsSurfaces[kNumBloomDsPass];      // downsample
        static BloomSurface s_bloomUsSurfaces[kNumBloomDsPass];      // upsample
        BloomPassInputs m_inputs;
    };

    struct PostProcessResolveInputs
    {
        float exposure;
        float bloom;
        float bloomIntensity;
        Texture* sceneTexture;
        Texture* bloomTexture;
    };

    // TODO: is use of static member here a bind design? as these
    // static members lives in the data section of the program image,
    // would this heart cache performance when calling render()
    struct PostProcessResolvePass : public RenderPass
    {
        PostProcessResolvePass(RenderTarget* rt, Viewport vp, PostProcessResolveInputs inputs);

        static void onInit();
        virtual void render() override;

        PostProcessResolveInputs m_inputs;
        static Shader* s_finalCompositeShader;
        static MaterialInstance* s_matl;
    };

    struct MeshPass : public RenderPass
    {
        static void onInit();
        virtual void render() = 0;

        MeshInstance* m_mesh;
        MaterialInstance* m_matl;
        glm::mat4 m_modelMatrix;
    };

    struct TexturedQuadPass : public RenderPass
    {
        TexturedQuadPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture);
        static void onInit();
        virtual void render() override;

        static Shader* m_shader;
        static MaterialInstance* m_matl;
        Texture* m_srcTexture;
    };

    struct LinePass : public RenderPass
    {
        // mesh
        virtual void render() override {};
    };

    struct RenderPassInput
    {
        enum Type
        {
            Scene = 0,
            Bloom,
            PostProcessResolve
        };
    };

    struct DebugAABBPassInput : public RenderPassInput
    {
        float a;
        float b; 
    };

    class RenderPassFactory
    {
    public:
        virtual RenderPass* createRenderPass() { return 0; }
        virtual RenderPass* createRenderPass(RenderTarget* renderTarget, Viewport viewport);
        virtual RenderPass* createRenderPass(RenderTarget* renderTarget, Viewport viewport, Scene* scene) { return  0; }
    };
}