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

    struct SceneView
    {
        SceneView(const Scene& scene, ICamera* inCamera, u32 flags, Texture2DRenderable* dstRenderTexture, const Viewport& dstViewport)
            : camera(inCamera), renderTexture(dstRenderTexture), viewport(dstViewport)
        {
            for (auto entity : scene.entities)
            {
                if ((entity->getProperties() & flags) == flags)
                {
                    entities.push_back(entity);
                }
            }
        }

        ICamera* camera = nullptr;
        Texture2DRenderable* renderTexture = nullptr;
        Viewport viewport;
        std::vector<Entity*> entities;
    };

    enum class SceneSsboBindings
    {
        kViewData = 0,
        DirLightData,
        PointLightsData,
        TransformMatrices,
        VctxGlobalData,
        kCount
    };

    enum class SceneColorBuffers
    {
        kSceneColor = 0,
        kDepth,
        kNormal,
        kCount
    };

    enum class SceneTextureBindings
    {
        SkyboxDiffuse = 0,
        SkyboxSpecular,
        BRDFLookupTexture,
        IrradianceProbe,
        ReflectionProbe,
        SSAO,
        SunShadow,
        VoxelGridAlbedo = (i32)SunShadow + 4,
        VoxelGridNormal,
        VoxelGridRadiance,
        VoxelGridOpacity,
        VctxOcclusion,
        VctxIrradiance,
        VctxReflection,
        kCount
    };

    // forward declarations
    struct RenderTarget;

    struct GfxPipelineState
    {
        DepthControl depth = DepthControl::kEnable;
        PrimitiveMode primitiveMode = PrimitiveMode::TriangleList;
    };

    /**
    * Encapsulate a mesh draw call, `renderTarget` should be configured to correct state, such as binding albedo buffers, and clearing
    * albedo buffers while `drawBuffers` for this draw call will be passed in.
    */
    using RenderSetupLambda = std::function<void(RenderTarget* renderTarget, Shader* shader)>;
    struct RenderTask
    {
        RenderTarget* renderTarget = nullptr;
        Viewport viewport = { };
        ISubmesh* submesh = nullptr;
        Shader* shader = nullptr;
        GfxPipelineState pipelineState;
        RenderSetupLambda renderSetupLambda = [](RenderTarget* renderTarget, Shader* shader) { };
    };

    class RenderResourceManager {
    public:
        Texture2DRenderable* allocTexture(const char* name, const ITextureRenderable& spec, const ITextureRenderable::Parameter& param) {
            /*
            for () {

            }
            */
        }

        void releaseTexture(Texture2DRenderable* toRecycle);
    private:
        std::vector<Texture2DRenderable*> reusableTextures;
        std::unordered_map<std::string, Texture2DRenderable*> textureMap;
    };

    class Renderer : public Singleton<Renderer>
    {
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

            void initialize() {}
            void update() {}
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

        /**
        * brief: renderScene() implemented using glMultiDrawIndirect()
        */
        struct IndirecDraw
        {
            GLuint buffer = -1;
            u32 sizeInBytes = 1024 * 1024 * 32;
            void* data = nullptr;
        } indirectDrawBuffer;
        // Texture2DRenderable* renderSceneMultiDraw(SceneRenderable& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution, const SSGITextures& SSGIOutput);
        void renderSceneMultiDraw(SceneRenderable& renderableScene, RenderTarget* outRenderTarget, Texture2DRenderable* outSceneColor, const SSGITextures& SSGIOutput);
        void submitSceneMultiDrawIndirect(const SceneRenderable& renderableScene);

        std::unique_ptr<ManyViewGI> m_manyViewGI = nullptr;
#if 0
        void gpuRayTracing(struct RayTracingScene& rtxScene, RenderTexture2D* outputBuffer, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer);
#endif
        void renderSceneMeshOnly(SceneRenderable& sceneRenderable, RenderTarget* dstRenderTarget, Shader* shader);

        struct ZPrepassOutput {
            Texture2DRenderable* depthBuffer;
            Texture2DRenderable* normalBuffer;
        };
        // ZPrepassOutput renderSceneDepthNormal(SceneRenderable& renderableScene, const glm::uvec2& outputResolution);
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

        // todo: implement this
        void renderDebugObjects();

        /* Shadow */
        void renderShadowMaps(const Scene& scene, const SceneRenderable& renderableScene);

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
        void submitMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& renderSetupLambda);

        /**
        * Draw a mesh using same material for all its submesh
        */
        void submitMaterialMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& perMeshSetupLambda);

        /**
        * 
        */
        void submitFullScreenPass(RenderTarget* renderTarget, Shader* shader, RenderSetupLambda&& renderSetupLambda);
        void submitScreenQuadPass(RenderTarget* renderTarget, Viewport viewport, Shader* shader, RenderSetupLambda&& renderSetupLambda);

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(RenderTask&& task);
//

// post-processing
        // gaussian blur
        void gaussianBlur(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma);

        // taa
        glm::vec2 TAAJitterVectors[16] = { };
        f32 reconstructionWeights[16] = { };
        glm::mat4 m_originalProjection = glm::mat4(1.f);
        Texture2DRenderable* m_TAAOutput = nullptr;
        RenderTarget* m_TAAPingPongRenderTarget[2] = { 0 };
        Texture2DRenderable* m_TAAPingPongTextures[2] = { 0 };
        void taa();

        struct DownsampleChain
        {
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

// per frame data
        void updateVctxData(Scene* scene);
//
        BoundingBox3D computeSceneAABB(Scene* scene);

        enum class TonemapOperator
        {
            kReinhard = 0,
            kACES,
            kCount
        };

        struct Settings
        {
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
            f32  exposure = 1.f;
            f32 bloomIntensity = 0.7f;
            f32 colorTempreture = 6500.f;
        } m_settings;

        glm::uvec2 m_windowSize;
        glm::uvec2 m_offscreenRenderSize;

        // normal hdr scene albedo texture 
        Texture2DRenderable* m_sceneColorTexture;
        Texture2DRenderable* m_sceneNormalTexture;
        Texture2DRenderable* m_sceneDepthTexture;
        RenderTarget* m_sceneColorRenderTarget;

        // hdr super sampling albedo buffer
        Texture2DRenderable* m_sceneColorTextureSSAA;
        Texture2DRenderable* m_sceneNormalTextureSSAA;
        RenderTarget* m_sceneColorRTSSAA;
        Texture2DRenderable* m_sceneDepthTextureSSAA;
        Shader* m_sceneDepthNormalShader;
        Texture2DRenderable* m_finalColorTexture;

        // voxel cone tracing
        struct VoxelGrid
        {
            // todo: for some reason settings this to 128 crashes RenderDoc every time
            const u32 resolution = 128u;
            // const u32 resolution = 8u;
            Texture3DRenderable* albedo = nullptr;
            Texture3DRenderable* normal = nullptr;
            Texture3DRenderable* emission = nullptr;
            Texture3DRenderable* radiance = nullptr;
            Texture3DRenderable* opacity = nullptr;
            glm::vec3 localOrigin;
            f32 voxelSize;
        } m_sceneVoxelGrid;

        struct Vctx
        {
            static const u32 ssaaRes = 4u;

            struct Opts
            {
                // offset for ray origin
                f32 coneOffset = 2.f;         
                // toggle on/off super sampled scene opacity
                f32 useSuperSampledOpacity = 1.f;
                // scale used to scale opacity's contribution to occlusion
                f32 occlusionScale = 1.5f; 
                // scale the opacity of each voxel or filtered voxel
                f32 opacityScale = 1.f;
                // indirect lighting scale
                f32 indirectScale = 1.f;
                // bool superSampled = true;
            } opts;

            struct Voxelizer
            {
                void init(u32 resolution);

                RenderTarget* renderTarget = nullptr;
                // super sampled render target
                RenderTarget* ssRenderTarget = nullptr;
                Texture2DRenderable* colorBuffer = nullptr;
                Shader* voxelizeShader = nullptr;
                Shader* filterVoxelGridShader = nullptr;
                GLuint opacityMaskSsbo = -1;
            } voxelizer;

            struct Visualizer
            {
                void init();

                enum class Mode
                {
                    kAlbedo = 0,
                    kOpacity,
                    kRadiance,
                    kNormal,
                    kOpacitySS,
                    kCount
                } mode = Mode::kAlbedo;

                const char* vctxVisModeNames[(u32)Mode::kCount] = { 
                    "albedo",
                    "opacity",
                    "radiance",
                    "normal",
                    "opacitySS"
                };

                // help visualize a traced cone
                i32 activeMip = 0u;
                GLuint ssbo = -1;
                GLuint idbo = -1;
                const static u32 kMaxNumCubes = 1024u;
                glm::vec2 debugScreenPos = glm::vec2(-0.56f, 0.32f);
                bool debugScreenPosMoved = false;
                bool cachedTexInitialized = false;
                glm::mat4 cachedView = glm::mat4(1.f);
                glm::mat4 cachedProjection = glm::mat4(1.f);
                Shader* coneVisDrawShader = nullptr;
                Shader* coneVisComputeShader = nullptr;
                Texture2DRenderable* cachedSceneDepth = nullptr;
                Texture2DRenderable* cachedSceneNormal = nullptr;

                Shader* voxelGridVisShader = nullptr;
                RenderTarget* renderTarget = nullptr;
                Texture2DRenderable* colorBuffer = nullptr;

                struct DebugBuffer
                {
                    struct ConeCube
                    {
                        glm::vec3 center;
                        f32 size;
                        glm::vec4 albedo;
                    };

                    i32 numCubes = 0;
                    glm::vec3 padding;
                    ConeCube cubes[kMaxNumCubes] = { };
                } debugConeBuffer;
            } visualizer;

            enum class ColorBuffers
            {
                kOcclusion = 0,
                kIrradiance,
                kReflection
            };

            RenderTarget* renderTarget = nullptr;
            Shader* renderShader = nullptr;
            Shader* resolveShader = nullptr;
            Texture2DRenderable* occlusion = nullptr;
            Texture2DRenderable* shared = nullptr;
            Texture2DRenderable* reflection = nullptr;
        } m_vctx;

        struct VctxGpuData
        {
            glm::vec3 localOrigin;
            f32 voxelSize;
            u32 visMode;
            glm::vec3 padding;
        } m_vctxGpuData;
        GLuint m_vctxSsbo;

        /*
        * Voxelize a given scene
        */
        void voxelizeScene(Scene* scene);
        void renderVctx();

        /*
        * Visualization to help debug voxel cone tracing
        */
        void visualizeVoxelGrid();
        void visualizeConeTrace(Scene* scene);

        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;
    private:
        GfxContext* m_ctx;
        u32 m_numFrames = 0u;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        Texture2DRenderable* m_rtxPingPongBuffer[2] = { 0 };
        bool bVisualize = false;
    };
};
