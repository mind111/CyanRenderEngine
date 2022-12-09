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
    struct SceneRenderable;

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

    struct GfxPipelineState {
        DepthControl depth = DepthControl::kEnable;
        PrimitiveMode primitiveMode = PrimitiveMode::TriangleList;
    };

    /**
    * Encapsulate a mesh draw call, `renderTarget` should be configured to correct state, such as binding albedo buffers, and clearing
    * albedo buffers while `drawBuffers` for this draw call will be passed in.
    */
    using RenderSetupLambda = std::function<void(RenderTarget* renderTarget, Shader* shader)>;
    struct RenderTask {
        RenderTarget* renderTarget = nullptr;
        Viewport viewport = { };
        ISubmesh* submesh = nullptr;
        Shader* shader = nullptr;
        GfxPipelineState pipelineState;
        RenderSetupLambda renderSetupLambda = [](RenderTarget* renderTarget, Shader* shader) { };
    };

    class Renderer : public Singleton<Renderer> {
    public:
        using UIRenderCommand = std::function<void()>;

        friend class InstantRadiosity;

        explicit Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight);
        ~Renderer() {}

        void initialize();
        void finalize();

        GfxContext* getGfxCtx() { return m_ctx; };
        LinearAllocator& getFrameAllocator() { return m_frameAllocator; }

// rendering
        void beginRender();
        void render(Scene* scene, const SceneView& sceneView);
        void renderToScreen(Texture2DRenderable* inTexture);
        void endRender();

        struct SceneTextures {
            glm::uvec2 resolution;
            Texture2DRenderable* depth = nullptr;
            Texture2DRenderable* normal = nullptr;
            Texture2DRenderable* color = nullptr;
            Texture2DRenderable* postProcessed = nullptr;
            RenderTarget* renderTarget = nullptr;
            bool bInitialized = false;

            void initialize(const glm::uvec2& inResolution);
        } m_sceneTextures;

        Texture2DRenderable* renderScene(SceneRenderable& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution);

        /**
        * brief:
        * screen space ray tracing
        */
        struct SSGITextures
        {
            Texture2DRenderable* ao = nullptr;
            Texture2DRenderable* bentNormal = nullptr;
            Texture2DRenderable* shared = nullptr;
        };
        SSGITextures screenSpaceRayTracing(Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture, const glm::uvec2& renderResolution);

        struct IndirectDrawBuffer
        {
            IndirectDrawBuffer();
            ~IndirectDrawBuffer() { }

            GLuint buffer = -1;
            u32 sizeInBytes = 1024 * 1024 * 32;
            void* data = nullptr;
        } indirectDrawBuffer;

        /**
        * brief: renderScene() implemented using glMultiDrawIndirect()
        */
        void renderSceneBatched(SceneRenderable& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outSceneColor, const SSGITextures& SSGIOutput);
        void submitSceneMultiDrawIndirect(const SceneRenderable& renderableScene);

        std::unique_ptr<ManyViewGI> m_manyViewGI = nullptr;

        struct ZPrepassOutput {
            Texture2DRenderable* depthBuffer;
            Texture2DRenderable* normalBuffer;
        };
        void renderSceneDepthNormal(SceneRenderable& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outDepthBuffer, Texture2DRenderable* outNormalBuffer);
        void renderSceneDepthOnly(SceneRenderable& renderableScene, Texture2DRenderable* outDepthTexture);

        bool bFullscreenRadiosity = false;
        InstantRadiosity m_instantRadiosity;

        /* Debugging utilities */
        void debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1);
        void debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection);
        void debugDrawCubemap(TextureCubeRenderable* cubemap);
        void debugDrawCubemap(GLuint cubemap);

        /** note - @min:
        */
        void renderShadowMaps(SceneRenderable& scene);

        /*
        * Render provided scene into a light probe
        */
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);

        void addUIRenderCommand(const std::function<void()>& UIRenderCommand);

        /*
        * brief: helper functions for appending to the "Rendering" tab
        */
        void appendToRenderingTab(const std::function<void()>& command);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI();

        void drawMeshInstance(const SceneRenderable& renderableScene, RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex);

        /**
        * Draw a mesh without material
        */
        void drawMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& renderSetupLambda);

        /**
        * 
        */
        void drawFullscreenQuad(RenderTarget* renderTarget, Shader* shader, RenderSetupLambda&& renderSetupLambda);
        void drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, Shader* shader, RenderSetupLambda&& renderSetupLambda);

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(RenderTask&& task);
//

// post-processing
        // gaussian blur
        void gaussianBlur(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma);

        struct DownsampleChain {
            std::vector<RenderTexture2D*> stages;
            u32 numStages;
        };
        DownsampleChain downsample(RenderTexture2D* inTexture, u32 inNumStages);
        RenderTexture2D* bloom(RenderTexture2D* inTexture);

        Texture2DRenderable* downsample(Texture2DRenderable* inTexture, Texture2DRenderable* outTexture);

        /**
        * Local tonemapping using "Exposure Fusion"
        * https://bartwronski.com/2022/02/28/exposure-fusion-local-tonemapping-for-real-time-rendering/
        * https://mericam.github.io/papers/exposure_fusion_reduced.pdf 
        */
        void localToneMapping();

        /*
        * Compositing and resolving to final output albedo texture. Applying bloom, tone mapping, and gamma correction.
        */
        void composite(Texture2DRenderable* composited, Texture2DRenderable* inSceneColor, Texture2DRenderable* inBloomColor, const glm::uvec2& outputResolution);
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

        BoundingBox3D computeSceneAABB(Scene* scene);

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
        bool bVisualize = false;
    };
};
