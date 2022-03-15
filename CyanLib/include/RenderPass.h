#pragma once

#include "scene.h"
#include "Shader.h"
#include "GfxContext.h"
#include "RenderTarget.h"
#include "Geometry.h"

namespace Cyan
{
    QuadMesh* getQuadMesh();
#if 0
    void onRendererInitialized(glm::vec2 renderSize);

    // TODO: make RenderPass persistent, instead of scratching and recreating again every frame
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
        static Shader* s_sceneNormalDepthShader;
        static Texture* ssaoBlurHoriTex;
        static Texture* ssaoBlurVertTex;
        static RenderTarget* ssaoBlurRt;
    };

    struct BloomPassInput
    {
        // TODO: output should really just be a texture
        Texture* sceneTexture;
        Texture* bloomOutputTexture;
    };

    struct GaussianBlurInput
    {
        i32 kernelIndex;
        i32 radius;
    };

    struct BloomPass : public RenderPass
    {
        BloomPass(RenderTarget* rt, Viewport vp, BloomPassInput inputs); 
        ~BloomPass() { }

        static void onInit(u32 windowWidth, u32 windowHeight);
        virtual void render() override;
        static Texture* getBloomOutput();

        struct BloomSurface
        {
            RenderTarget* m_renderTarget;
            Texture* m_pingPongColorBuffers[2];
        };

        void downSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx);
        void bloomDownSample();
        void upScale(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx, RenderTarget* blend, u32 blendIdx, u32 stageIdx);
        void bloomUpscale();
        void gaussianBlur(BloomSurface src, GaussianBlurInput inputs);

        static const u32 kNumBloomDsPass = 6u;
        // setup
        static Shader* s_bloomSetupShader;
        static MaterialInstance* s_bloomSetupMatl;
        // downsample
        static Shader* s_bloomDsShader;
        static MaterialInstance* s_bloomDsMatl;
        // upscale
        static Shader* s_bloomUsShader;
        static MaterialInstance* s_bloomUsMatl;
        // gaussian blur
        static Shader* s_gaussianBlurShader;
        static MaterialInstance* s_gaussianBlurMatl;
        static RenderTarget* s_bloomSetupRT;                         // preprocess surface
        static BloomSurface s_bloomDsSurfaces[kNumBloomDsPass];      // downsample
        static BloomSurface s_bloomUsSurfaces[kNumBloomDsPass];      // upsample
        BloomPassInput m_inputs;
    };

    struct PostProcessResolveInput
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
        PostProcessResolvePass(RenderTarget* rt, Viewport vp, PostProcessResolveInput inputs);

        static void onInit();
        virtual void render() override;

        PostProcessResolveInput m_inputs;
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

    struct SSAOPass : public RenderPass
    {
        SSAOPass(RenderTarget* renderTarget, Viewport viewport, Texture* sceneDepthTexture, Texture* sceneNormalTexture);
        static void onInit();
        virtual void render() override;

        static Shader* m_ssaoShader;
        static MaterialInstance* m_ssaoMatl;
        static Texture* m_ssaoTexture;
        Texture* m_sceneDepthTexture;
        Texture* m_sceneNormalTexture;
    };

    struct GaussianBlurPass : public RenderPass
    {
        GaussianBlurPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTex, Texture* horizontal, Texture* vertical, GaussianBlurInput params);
        static void onInit();
        virtual void render() override;
        Texture* getBlurOutput();

        static Shader* m_gaussianBlurShader;
        static MaterialInstance* m_gaussianBlurMatl;
        RenderTarget* m_blurRenderTarget;
        i32 m_drawBuffers[8];
        u32 m_numDrawBuffers;
        Texture* m_srcTexture;
        Texture* m_horiztontalTex;
        Texture* m_verticalTex;
        GaussianBlurInput m_params;
    };

    // TODO: min/max depth for each cascade to help shrink the orthographic projection size
    // TODO: blend between cascades to alleviate artifact when transitioning between cascades
    // TODO: better shadow biasing; normal bias and receiver geometry bias
    // TODO: distribution based shadow mapping methods: VSM, ESM etc.
    /*
    struct DirectionalShadowPass : public RenderPass
    {
        DirectionalShadowPass(RenderTarget* renderTarget, Viewport viewport, const Camera& camera, const DirectionalLight& light, const std::vector<Entity*>& shadowCasters);
        static void onInit();
        virtual void render() override;
        void renderCascade(ShadowCascade& cascade, glm::mat4& lightView);
        static void computeCascadeAABB(ShadowCascade& cascade, const Camera& camera, glm::mat4& view);

        static Shader* s_directShadowShader;
        static MaterialInstance* s_directShadowMatl;
        static RenderTarget* s_depthRenderTarget;
        static const u32 kNumShadowCascades = 4u;
        static CascadedShadowmap m_cascadedShadowMap;
        static Texture* m_horizontalBlurTex;
        static Texture* m_verticalBlurTex;

        void drawDebugLines();

        Camera m_camera;
        DirectionalLight m_light;
        const std::vector<Entity*>& m_shadowCasters;
    };
    */

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
#endif
}