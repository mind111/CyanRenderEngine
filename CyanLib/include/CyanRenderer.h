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
#include "camera.h"
#include "Entity.h"
#include "Geometry.h"
#include "Shadow.h"
#include "InstantRadiosity.h"
#include "ManyViewGI.h"

namespace Cyan
{
    struct RenderableScene;

    struct SceneView {
        SceneView(
            const Scene& scene, 
            const PerspectiveCamera& inCamera, 
            const std::function<bool(Entity*)>& selector = [](Entity* entity) {
                return true;
            },
            GfxTexture2D* dstRenderTexture = nullptr, 
                const Viewport& dstViewport = { })
            : camera(inCamera), renderTexture(dstRenderTexture), viewport(dstViewport) {
            for (auto entity : scene.m_entities) {
                if (selector(entity)) {
                    entities.push_back(entity);
                }
            }
        }

        PerspectiveCamera camera;
        GfxTexture2D* renderTexture = nullptr;
        Viewport viewport;
        std::vector<Entity*> entities;
    };

    // forward declarations
    struct RenderTarget;

    struct GfxPipelineConfig {
        DepthControl depth = DepthControl::kEnable;
        PrimitiveMode primitiveMode = PrimitiveMode::TriangleList;
    };

    /**
    * Encapsulate a mesh draw call, `renderTarget` should be configured to correct state, such as binding albedo buffers, and clearing
    * albedo buffers while `drawBuffers` for this draw call will be passed in.
    */
    using RenderSetupLambda = std::function<void(VertexShader* vs, PixelShader* ps)>;
    struct RenderTask 
    {
        RenderTarget* renderTarget = nullptr;
        Viewport viewport = { };
        StaticMesh::Submesh* submesh = nullptr;
        PixelPipeline* pipeline = nullptr;
        GfxPipelineConfig config;
        RenderSetupLambda renderSetupLambda = [](VertexShader* vs, PixelShader* ps) {};
    };

    struct CachedTexture2D
    {
        CachedTexture2D() = default;
        CachedTexture2D(const CachedTexture2D& src)
            : texture(src.texture), refCount(src.refCount)
        {
            (*refCount)++;
        }

        CachedTexture2D(const char* name, const GfxTexture2D::Spec& spec, const Sampler2D& inSampler = {})
            : refCount(nullptr), texture(nullptr)
        {
            refCount = new u32(1u);

            // search in the texture cache
            auto entry = cache.find(spec);
            if (entry == cache.end())
            {
                // create a new texture and add it to the cache
                texture = GfxTexture2D::create(spec, inSampler);
            }
            else
            {
                // recycle a texture and remove it from the cache
                texture = entry->second;
                cache.erase(entry);
            }
        }

        ~CachedTexture2D()
        {
            (*refCount)--;
            if (*refCount == 0u)
            {
                release();
                delete refCount;
            }
        }

        CachedTexture2D& operator=(const CachedTexture2D& src)
        {
            refCount = src.refCount;
            (*refCount)++;
            texture = src.texture;
            return *this;
        }

        GfxTexture2D* get()
        {
            return texture;
        }

        GfxTexture2D* operator->()
        {
            return texture;
        }

        u32* refCount = nullptr;
        GfxTexture2D* texture = nullptr;
    private:
        void release()
        {
            cache.insert({ texture->getSpec(), texture });
            texture = nullptr;
        }
        
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<GfxTexture2D::Spec, GfxTexture2D*> cache;
    };

    struct HiZBuffer
    {
        HiZBuffer(const GfxTexture2D::Spec& spec);
        ~HiZBuffer() { }

        void build(GfxTexture2D* srcDepthTexture);

        std::unique_ptr<GfxTexture2D> texture;
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
        void render(Scene* scene, const SceneView& sceneView, const glm::uvec2& renderResolution);
        void renderToScreen(GfxTexture2D* inTexture);
        void endRender();

        struct GBuffer
        {
            GfxTexture2D* depth = nullptr;
            GfxTexture2D* normal = nullptr;
            GfxTexture2D* albedo = nullptr;
            GfxTexture2D* metallicRoughness = nullptr;
        };

        struct SceneTextures 
        {
            glm::uvec2 resolution;
            GBuffer gBuffer;
            GfxTexture2D* directDiffuseLighting = nullptr;
            GfxTexture2D* directLighting = nullptr;
            GfxTexture2D* indirectLighting = nullptr;
            GfxTexture2D* ssgiMirror = nullptr;
            GfxTexture2D* color = nullptr;
            GfxTexture2D* ao = nullptr;
            GfxTexture2D* bentNormal = nullptr;
            GfxTexture2D* irradiance = nullptr;
            RenderTarget* renderTarget = nullptr;
            HiZBuffer* HiZ = nullptr;

            bool bInitialized = false;

            void initialize(const glm::uvec2& inResolution);
        } m_sceneTextures;

        struct SSGI
        {
            struct HitBuffer
            {
                HitBuffer(u32 inNumLayers, const glm::uvec2& resolution);
                ~HitBuffer() { }

                GfxTexture2DArray* position = nullptr;
                GfxTexture2DArray* normal = nullptr;
                GfxTexture2DArray* radiance = nullptr;
                GLuint positionArray;
                GLuint normalArray;
                GLuint radianceArray;
                u32 numLayers;
            };

            SSGI(Renderer* renderer, const glm::uvec2& inRes);
            ~SSGI() { };

            // basic brute force hierarchical tracing without spatial ray reuse
            void render(GfxTexture2D* outAO, GfxTexture2D* outBentNormal, GfxTexture2D* outIrradiance, const GBuffer& gBuffer, HiZBuffer* HiZ, GfxTexture2D* inDirectDiffuseBuffer);
            // spatial reuse
            void renderEx(GfxTexture2D* outAO, GfxTexture2D* outBentNormal, GfxTexture2D* outIrradiance, const GBuffer& gBuffer, HiZBuffer* HiZ, GfxTexture2D* inDirectDiffuseBuffer);
            // todo: spatio-temporal reuse

            static const u32 kNumSamples = 8u;
            static const u32 kNumIterations = 64u;
            i32 numReuseSamples = 8;
            f32 reuseKernelRadius = .05f;
            glm::vec2 resolution;
            HitBuffer hitBuffer;
            Renderer* renderer = nullptr;
        } m_ssgi;

        struct PostProcessingTextures 
        {
            glm::uvec2 resolution;
            GfxTexture2D* bloom[5] = { };
            
            bool bInitialized = false;

            void initialize();
        } m_postProcessingTextures;

        // managing creating and recycling render target
        RenderTarget* createCachedRenderTarget(const char* name, u32 width, u32 height);
        GfxTexture2D* renderScene(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution);

        struct IndirectDrawBuffer
        {
            IndirectDrawBuffer();
            ~IndirectDrawBuffer() { }

            GLuint buffer = -1;
            u32 sizeInBytes = 1024 * 1024 * 32;
            void* data = nullptr;
        } indirectDrawBuffer;
        void multiDrawSceneIndirect(const RenderableScene& renderableScene);

        void renderSceneBatched(RenderableScene& renderableScene, RenderTarget* outRenderTarget, GfxTexture2D* outSceneColor);
        void renderSceneDepthPrepass(RenderableScene& renderableScene, RenderTarget* outRenderTarget, GfxTexture2D* outDepthBuffer);
        void renderSceneDepthOnly(RenderableScene& renderableScene, DepthTexture2D* outDepthTexture);
        void renderSceneGBuffer(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer);
        void renderSceneGBufferWithTextureAtlas(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer);
        void renderShadowMaps(RenderableScene& scene);
        void renderSceneLighting(RenderTarget* outRenderTarget, GfxTexture2D* outSceneColor, RenderableScene& scene, GBuffer gBuffer);
        void renderSceneDirectLighting(RenderTarget* outRenderTarget, GfxTexture2D* outDirectLighting, RenderableScene& scene, GBuffer gBuffer);
        void renderSceneIndirectLighting(RenderTarget* outRenderTarget, GfxTexture2D* outIndirectLighting, RenderableScene& scene, GBuffer gBuffer);

        bool bDebugSSRT = false;
        glm::vec2 debugCoord = glm::vec2(.5f);
        static const i32 kNumIterations = 64;
        i32 numDebugRays = 8;
        void legacyScreenSpaceRayTracing(GfxTexture2D* depth, GfxTexture2D* normal);
        void visualizeSSRT(GfxTexture2D* depth, GfxTexture2D* normal);

        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);
        void drawMesh(RenderTarget* renderTarget, Viewport viewport, StaticMesh* mesh, PixelPipeline* pipeline, const RenderSetupLambda& renderSetupLambda, const GfxPipelineConfig& config = GfxPipelineConfig{});
        void drawFullscreenQuad(RenderTarget* renderTarget, PixelPipeline* pipeline, const RenderSetupLambda& renderSetupLambda);
        void drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, PixelPipeline* pipeline, RenderSetupLambda&& renderSetupLambda);
        void drawColoredScreenSpaceQuad(RenderTarget* renderTarget, const glm::vec2& screenSpaceMin, const glm::vec2& screenSpaceMax, const glm::vec4& color);
        void blitTexture(GfxTexture2D* dst, GfxTexture2D* src);

        struct Vertex
        {
            glm::vec4 position;
            glm::vec4 color;
        };
        void drawScreenSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices);
        void drawWorldSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices);
        void drawWorldSpacePoints(RenderTarget* renderTarget, const std::vector<Vertex>& points);
        std::queue<std::function<void(void)>> debugDrawCalls;
        void drawDebugObjects();

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(const RenderTask& task);

        /* Debugging utilities */
        void debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1);
        void debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubemap(TextureCube* cubemap);
        void debugDrawCubemap(GLuint cubemap);

        void addUIRenderCommand(const std::function<void()>& UIRenderCommand);

        /*
        * brief: helper functions for appending to the "Rendering" tab
        */
        void appendToRenderingTab(const std::function<void()>& command);

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
            std::vector<GfxTexture2D*> images;
            GfxTexture2D* srcTexture;
        };
        void downsample(GfxTexture2D* src, GfxTexture2D* dst);
        void upscale(GfxTexture2D* src, GfxTexture2D* dst);
        CachedTexture2D bloom(GfxTexture2D* src);

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
            std::string name;
            GfxTexture2D* texture = nullptr;
            i32 activeMip = 0;
            bool* bSwitch = nullptr;
        };
        void setVisualization(VisualizationDesc* desc) { m_visualization = desc; }
        void visualize(GfxTexture2D* dst, GfxTexture2D* src, i32 mip = 0);
        VisualizationDesc* m_visualization = nullptr;
        void registerVisualization(const std::string& categoryName, const char* visName, GfxTexture2D* visualization, bool* toggle=nullptr);
        std::unordered_map<std::string, std::vector<VisualizationDesc>> visualizationMap;

        enum class TonemapOperator {
            kReinhard = 0,
            kACES,
            kSmoothstep,
            kCount
        };

        struct Settings {
            bool enableSunShadow = true;
            bool bSSAOEnabled = true;
            bool bBentNormalEnabled = true;
            bool bIndirectIrradianceEnabled = true;
            bool enableBloom = true;
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
        glm::uvec2 m_offscreenRenderSize;
        bool bFixDebugRay = false;

    private:

        GfxContext* m_ctx;
        u32 m_numFrames = 0u;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        std::unique_ptr<ManyViewGI> m_manyViewGI = nullptr;
        std::vector<GfxTexture2D*> renderTextures;
        bool bVisualize = false;
    };
};
