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

namespace Cyan
{
    struct RenderableScene;

    struct SceneView 
    {
        SceneView(Scene* inScene, Camera* inCamera, GfxTexture2D* renderOutput)
            : viewedScene(inScene), camera(inCamera), canvas(renderOutput)
        {

        }

        Scene* viewedScene = nullptr;
        Camera* camera = nullptr;
        GfxTexture2D* canvas = nullptr;
    };

    // forward declarations
    struct RenderTarget;

    struct GfxPipelineConfig 
    {
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

    struct RenderTexture2D
    {
        RenderTexture2D() = default;
        RenderTexture2D(const RenderTexture2D& src)
            : texture(src.texture), refCount(src.refCount)
        {
            (*refCount)++;
        }

        RenderTexture2D(const char* inName, const GfxTexture2D::Spec& spec, const Sampler2D& inSampler = {})
            : name(inName), refCount(nullptr), texture(nullptr)
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

        ~RenderTexture2D()
        {
            (*refCount)--;
            if (*refCount == 0u)
            {
                release();
                delete refCount;
            }
        }

        RenderTexture2D& operator=(const RenderTexture2D& src)
        {
            refCount = src.refCount;
            (*refCount)++;
            texture = src.texture;
            return *this;
        }

        static u32 getNumAllocatedGfxTexture2D() { return cache.size(); }

        GfxTexture2D* getGfxTexture2D() const
        {
            return texture;
        }

        std::string name;

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

        u32* refCount = nullptr;
        GfxTexture2D* texture = nullptr;
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
        void render(Scene* scene, const SceneView& sceneView);
        void renderToScreen(GfxTexture2D* inTexture);
        void endRender();

        struct GBuffer
        {
            GBuffer(const glm::uvec2& resolution);
            ~GBuffer() { }

            RenderTexture2D depth;
            RenderTexture2D normal;
            RenderTexture2D albedo;
            RenderTexture2D metallicRoughness;
        };

        struct SceneTextures
        {
            ~SceneTextures() { }

            static SceneTextures* create(const glm::uvec2& inResolution);

            glm::uvec2 resolution;
            GBuffer gBuffer;
            RenderTexture2D directLighting;
            RenderTexture2D directDiffuseLighting;
            RenderTexture2D indirectLighting;
            RenderTexture2D color;
            RenderTexture2D ssgiMirror;
            RenderTexture2D ao;
            RenderTexture2D bentNormal;
            RenderTexture2D irradiance;

            RenderTarget* renderTarget = nullptr;
            HiZBuffer* HiZ = nullptr;

        private:
            SceneTextures(const glm::uvec2& inResolution);
        };
        
        SceneTextures* m_sceneTextures = nullptr;

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
            void render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, HiZBuffer* HiZ, RenderTexture2D inDirectDiffuseBuffer);
            // spatial reuse
            void renderEx(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, HiZBuffer* HiZ, RenderTexture2D inDirectDiffuseBuffer);
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

        void renderSceneDepthOnly(RenderableScene& renderableScene, GfxDepthTexture2D* outDepthTexture);
        void renderSceneGBufferWithTextureAtlas(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer);
        void renderShadowMaps(Scene* inScene);
        void renderSceneDepthPrepass(const RenderableScene& renderableScene, RenderTarget* outRenderTarget, RenderTexture2D outDepthBuffer);
        void renderSceneGBuffer(RenderTarget* outRenderTarget, const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneLighting(RenderTexture2D outSceneColor, const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneDirectLighting(RenderTexture2D outDirectLighting, const RenderableScene& scene, GBuffer gBuffer);
        void renderSceneIndirectLighting(RenderTexture2D outIndirectLighting, const RenderableScene& scene, GBuffer gBuffer);

        bool bDebugSSRT = false;
        glm::vec2 debugCoord = glm::vec2(.5f);
        static const i32 kNumIterations = 64;
        i32 numDebugRays = 8;
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
        glm::uvec2 m_offscreenRenderSize;
        bool bFixDebugRay = false;
    private:
        GfxContext* m_ctx;
        u32 m_numFrames = 0u;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        std::vector<GfxTexture2D*> renderTextures;
        bool bVisualize = false;
    };
};
