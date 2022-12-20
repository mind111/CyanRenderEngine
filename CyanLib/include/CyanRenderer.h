#pragma once

#include <functional>
#include <map>
#include <stack>

#include "glew.h"
#include "stb_image.h"

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
            Texture2DRenderable* dstRenderTexture = nullptr, 
                const Viewport& dstViewport = { })
            : camera(inCamera), renderTexture(dstRenderTexture), viewport(dstViewport) {
            for (auto entity : scene.entities) {
                if (selector(entity)) {
                    entities.push_back(entity);
                }
            }
        }

        PerspectiveCamera camera;
        Texture2DRenderable* renderTexture = nullptr;
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
    struct RenderTask {
        RenderTarget* renderTarget = nullptr;
        Viewport viewport = { };
        ISubmesh* submesh = nullptr;
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

        CachedTexture2D(const char* name, const ITextureRenderable::Spec& spec, const ITextureRenderable::Parameter& params = {})
            : refCount(nullptr), texture(nullptr)
        {
            refCount = new u32(1u);

            // search in the texture cache
            auto entry = cache.find(spec);
            if (entry == cache.end())
            {
                // create a new texture and add it to the cache
                texture = new Texture2DRenderable(name, spec, params);
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

        Texture2DRenderable* get()
        {
            return texture;
        }

        Texture2DRenderable* operator->()
        {
            return texture;
        }

        u32* refCount = nullptr;
        Texture2DRenderable* texture = nullptr;
    private:
        void release()
        {
            cache.insert({ texture->getTextureSpec(), texture });
            texture = nullptr;
        }
        
        /** Note - @mind:
         * Using multi-map here because we want to allow duplicates, storing multiple textures having the same
         * spec. Do note that find() will return an iterator to any instance of key-value pair among all values
         * associated with that key, so the order of returned values are not guaranteed.
         */
        static std::unordered_multimap<ITextureRenderable::Spec, Texture2DRenderable*> cache;
    };

    class Renderer : public Singleton<Renderer> {
    public:
        using UIRenderCommand = std::function<void()>;


        friend class InstantRadiosity;

        explicit Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight);
        ~Renderer() {}

        void initialize();
        void deinitialize();

        GfxContext* getGfxCtx() { return m_ctx; };
        LinearAllocator& getFrameAllocator() { return m_frameAllocator; }

// rendering
        void beginRender();
        void render(Scene* scene, const SceneView& sceneView);
        void renderToScreen(Texture2DRenderable* inTexture);
        void endRender();

        struct SceneTextures 
        {
            glm::uvec2 resolution;
            Texture2DRenderable* depth = nullptr;
            Texture2DRenderable* normal = nullptr;
            Texture2DRenderable* color = nullptr;
            RenderTarget* renderTarget = nullptr;

            bool bInitialized = false;

            void initialize(const glm::uvec2& inResolution);
        } m_sceneTextures;

        struct PostProcessingTextures 
        {
            glm::uvec2 resolution;
            Texture2DRenderable* bloom[5] = { };
            
            bool bInitialized = false;

            void initialize();
        } m_postProcessingTextures;

        // managing creating and recycling render target
        RenderTarget* createCachedRenderTarget(const char* name, u32 width, u32 height);
        Texture2DRenderable* renderScene(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution);

        struct IndirectDrawBuffer
        {
            IndirectDrawBuffer();
            ~IndirectDrawBuffer() { }

            GLuint buffer = -1;
            u32 sizeInBytes = 1024 * 1024 * 32;
            void* data = nullptr;
        } indirectDrawBuffer;
        void submitSceneMultiDrawIndirect(const RenderableScene& renderableScene);

        struct SSGITextures
        {
            Texture2DRenderable* ao = nullptr;
            Texture2DRenderable* bentNormal = nullptr;
            Texture2DRenderable* shared = nullptr;
        };
        /**
        * brief:
        * screen space ray tracing
        */
        SSGITextures screenSpaceRayTracing(Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture, const glm::uvec2& renderResolution);

        /**
        * brief: renderScene() implemented using glMultiDrawIndirect()
        */
        void renderSceneBatched(RenderableScene& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outSceneColor, const SSGITextures& SSGIOutput);
        void renderSceneDepthNormal(RenderableScene& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outDepthBuffer, Texture2DRenderable* outNormalBuffer);
        void renderSceneDepthOnly(RenderableScene& renderableScene, Texture2DRenderable* outDepthTexture);
        void renderShadowMaps(RenderableScene& scene);
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);
        void drawMesh(RenderTarget* renderTarget, Viewport viewport, Mesh* mesh, PixelPipeline* pipeline, const RenderSetupLambda& renderSetupLambda, const GfxPipelineConfig& config = GfxPipelineConfig{});
        void drawFullscreenQuad(RenderTarget* renderTarget, PixelPipeline* pipeline, RenderSetupLambda&& renderSetupLambda);
        void drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, PixelPipeline* pipeline, RenderSetupLambda&& renderSetupLambda);

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(const RenderTask& task);

        /* Debugging utilities */
        void debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1);
        void debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubemap(TextureCubeRenderable* cubemap);
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
        void gaussianBlur(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma);

        struct ImagePyramid {
            u32 numLevels;
            std::vector<Texture2DRenderable*> images;
            Texture2DRenderable* srcTexture;
        };
        void downsample(Texture2DRenderable* src, Texture2DRenderable* dst);
        void upscale(Texture2DRenderable* src, Texture2DRenderable* dst);
        CachedTexture2D bloom(Texture2DRenderable* src);

        /**
        * Local tonemapping using "Exposure Fusion"
        * https://bartwronski.com/2022/02/28/exposure-fusion-local-tonemapping-for-real-time-rendering/
        * https://mericam.github.io/papers/exposure_fusion_reduced.pdf 
        */
        void localToneMapping() { }

        /*
        * Compositing and resolving to final output albedo texture. Applying bloom, tone mapping, and gamma correction.
        */
        void compose(Texture2DRenderable* composited, Texture2DRenderable* inSceneColor, Texture2DRenderable* inBloomColor, const glm::uvec2& outputResolution);
//
        void setVisualization(Texture2DRenderable* visualization) { m_visualization = visualization; }
        void visualize(Texture2DRenderable* dst, Texture2DRenderable* src);
        Texture2DRenderable* m_visualization = nullptr;
        void registerVisualization(const std::string& categoryName, Texture2DRenderable* visualization, bool* toggle=nullptr);
        struct VisualizationDesc {
            Texture2DRenderable* texture = nullptr;
            bool* bSwitch = nullptr;
        };
        std::unordered_map<std::string, std::vector<VisualizationDesc>> visualizationMap;

        enum class TonemapOperator {
            kReinhard = 0,
            kACES,
            kSmoothstep,
            kCount
        };

        struct Settings {
            bool enableAA = true;
            bool enableTAA = false;
            bool enableSunShadow = true;
            bool enableSSAO = false;
            bool enableVctx = false;
            bool autoFilterVoxelGrid = true;
            bool enableBloom = true;
            bool enableTonemapping = true;
            bool useBentNormal = true;
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

    private:
        GfxContext* m_ctx;
        u32 m_numFrames = 0u;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        std::unique_ptr<ManyViewGI> m_manyViewGI = nullptr;
        bool bVisualize = false;
    };
};
