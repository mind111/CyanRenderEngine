#pragma once

#include <functional>
#include <map>
#include <stack>

#include <glew/glew.h>
#include <stbi/stb_image.h>

#include "Singleton.h"
#include "Allocator.h"
#include "Common.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "SceneRender.h"
#include "Camera.h"
#include "Entity.h"
#include "Geometry.h"
#include "ShadowMaps.h"
#include "RenderTexture.h"
#include "SSGI.h"

namespace Cyan
{
    using ShaderSetupFunc = std::function<void(ProgramPipeline*)>;

    // forward declarations
    struct Framebuffer;
    struct RenderPass;

    struct GpuDebugMarker
    {
        GpuDebugMarker(const char* inMessage)
            : message(inMessage)
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
        }

        ~GpuDebugMarker()
        {
            glPopDebugGroup();
        }

        const char* message = nullptr;
    };
#define GPU_DEBUG_SCOPE(markerVar, markerMsg) GpuDebugMarker markerVar(markerMsg);

    struct HiZBuffer
    {
        HiZBuffer(const GfxTexture2D::Spec& spec);
        ~HiZBuffer() { }

        void build(GfxTexture2D* srcDepthTexture);

        RenderTexture2D texture;
    };

    struct GBuffer
    {
        GBuffer(const glm::uvec2& resolution);
        ~GBuffer() { }

        RenderDepthTexture2D depth;
        RenderTexture2D normal;
        RenderTexture2D albedo;
        RenderTexture2D metallicRoughness;
    };

    class Renderer : public Singleton<Renderer>
    {
    public:
        using UIRenderCommand = std::function<void()>;
        friend class InstantRadiosity;

        Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight);
        ~Renderer() {}

        void initialize();
        void deinitialize();

        GfxContext* getGfxCtx() { return m_ctx; };
        LinearAllocator& getFrameAllocator() { return m_frameAllocator; }

        GfxTexture2D* createRenderTexture(const char* textureName, const GfxTexture2D::Spec& inSpec, const Sampler2D& inSampler);

// rendering
        void beginRender();
#if 0
        void render(Scene* scene, const SceneView& sceneView);
#else
        void render(Scene* scene);
#endif
        void endRender();
#if 0
        void renderSceneDepthOnly(RenderableScene& renderableScene, GfxDepthTexture2D* outDepthTexture);
#ifdef BINDLESS_TEXTURE
        void renderSceneGBufferBindless(const RenderableScene& scene, GBuffer gBuffer);
#endif
        void renderSceneGBufferNonBindless(const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneDepthPrepass(const RenderableScene& renderableScene, RenderDepthTexture2D outDepthBuffer);
        void renderSceneGBufferWithTextureAtlas(Framebuffer* outFramebuffer, RenderableScene& scene, GBuffer gBuffer);
        void renderSceneLighting(RenderTexture2D outSceneColor, const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneDirectLighting(RenderTexture2D outDirectLighting, const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneIndirectLighting(RenderTexture2D outIndirectLighting, const RenderableScene& scene, GBuffer gBuffer);
#else
        void renderSceneDepth(GfxDepthTexture2D* outDepthBuffer, Scene* scene, const SceneRender::ViewParameters& viewParameters);
        void renderSceneDepthPrepass(GfxDepthTexture2D* outDepthBuffer, Scene* scene, const SceneRender::ViewParameters& viewParameters);
        void renderShadowMaps(Scene* scene, Camera* camera);
        void renderSceneGBuffer(GfxTexture2D* outAlbedo, GfxTexture2D* outNormal, GfxTexture2D* outMetallicRoughness, GfxDepthTexture2D* depth, Scene* scene, const SceneRender::ViewParameters& viewParameters);
        void renderSceneLighting(Scene* scene, SceneRender* render);
        void renderSceneDirectLighting(Scene* scene, SceneRender* render);
        void renderSceneIndirectLighting(Scene* scene, SceneRender* render);
#endif
        void renderToScreen(GfxTexture2D* inTexture);

        bool bDebugSSRT = false;
        glm::vec2 debugCoord = glm::vec2(.5f);
        static const i32 kNumIterations = 64;
        i32 numDebugRays = 8;
        void visualizeSSRT(GfxTexture2D* depth, GfxTexture2D* normal);

        void drawStaticMesh(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& setupRenderTarget, const Viewport& viewport, StaticMesh* mesh, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader, const GfxPipelineState& gfxPipelineState);
        void drawFullscreenQuad(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& renderTargetSetupLambda, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader);
        void drawScreenQuad(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& renderTargetSetupLamdba, const Viewport& viewport, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader);
        void drawColoredScreenSpaceQuad(GfxTexture2D* outTexture, const glm::vec2& screenSpaceMin, const glm::vec2& screenSpaceMax, const glm::vec4& color);
        void blitTexture(GfxTexture2D* dst, GfxTexture2D* src);

        /* Debugging utilities */
        struct Vertex
        {
            glm::vec4 m_position;
            glm::vec4 color;
        };
        void drawScreenSpaceLines(Framebuffer* framebuffer, const std::vector<Vertex>& vertices);
        void drawWorldSpaceLines(Framebuffer* framebuffer, const std::vector<Vertex>& vertices);
        void drawWorldSpacePoints(Framebuffer* framebuffer, const std::vector<Vertex>& points);
        std::queue<std::function<void(void)>> debugDrawCalls;
        void drawDebugObjects();
        void debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1);
        void debugDrawSphere(Framebuffer* framebuffer, const Viewport& viewport, const glm::vec3& m_position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeImmediate(Framebuffer* framebuffer, const Viewport& viewport, const glm::vec3& m_position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeBatched(Framebuffer* framebuffer, const Viewport& viewport, const glm::vec3& m_position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubemap(GfxTextureCube* cubemap);
        void debugDrawCubemap(GLuint cubemap);

        void addUIRenderCommand(const std::function<void()>& UIRenderCommand);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI();
//

// post-processing
        // gaussian blur
        void gaussianBlur(GfxTexture2D* inoutTexture, u32 inRadius, f32 inSigma);

        struct ImagePyramid {
            u32 numLevels;
            std::vector<GfxTexture2D*> m_images;
            GfxTexture2D* srcTexture;
        };
        void downsample(GfxTexture2D* src, GfxTexture2D* dst);
        void upscale(GfxTexture2D* src, GfxTexture2D* dst);
        RenderTexture2D bloom(GfxTexture2D* src);

        /**
        * Local tonemapping using "Exposure Fusion"
        * https://bartwronski.com/2022/02/28/exposure-fusion-local-tonemapping-for-real-time-rendering/
        * https://mericam.github.io/papers/exposure_fusion_reduced.pdf 
        */
        void localToneMapping() { }

        /*
        * Compositing and resolving to final output albedo texture. Applying bloom, tone mapping, and gamma correction.
        */
        void compose(GfxTexture2D* composited, GfxTexture2D* inSceneColor, GfxTexture2D* inBloomColor, const glm::uvec2& outputResolution);
//
        struct VisualizationDesc {
            std::string m_name;
            GfxTexture2D* texture = nullptr;
            i32 activeMip = 0;
            bool* bSwitch = nullptr;
        };
        void setVisualization(VisualizationDesc* desc) { m_visualization = desc; }
        void visualize(GfxTexture2D* dst, GfxTexture2D* src, i32 mip = 0);
        VisualizationDesc* m_visualization = nullptr;
        void registerVisualization(const std::string& categoryName, const char* visName, GfxTexture2D* visualization, bool* toggle=nullptr);
        std::unordered_map<std::string, std::vector<VisualizationDesc>> visualizationMap;

        enum class TonemapOperator 
        {
            kReinhard = 0,
            kACES,
            kSmoothstep,
            kCount
        };

        struct Settings 
        {
            bool enableSunShadow = true;
            bool bSSAOEnabled = true;
            bool bBentNormalEnabled = true;
            bool bIndirectIrradianceEnabled = true;
            bool enableBloom = false;
            bool enableTonemapping = true;
            bool bPostProcessing = true;
            bool bManyViewGIEnabled = false;
            u32 tonemapOperator = (u32)TonemapOperator::kReinhard;
            f32 whitePointLuminance = 100.f;
            f32 smoothstepWhitePoint = 1.f;
            f32  exposure = 1.f;
            f32 bloomIntensity = 0.7f;
            f32 colorTempreture = 6500.f;
        } m_settings;

        glm::uvec2 m_windowSize;
        bool bFixDebugRay = false;
    private:
        GfxContext* m_ctx;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        std::vector<GfxTexture2D*> renderTextures;
        bool bVisualize = false;
        std::shared_ptr<StaticMesh> m_quad = nullptr;
    };
};
