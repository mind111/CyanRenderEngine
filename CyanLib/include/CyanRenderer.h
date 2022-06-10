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


namespace Cyan
{
    struct SceneRenderable;

    struct SceneView
    {
        Camera camera = { };
        RenderTarget* renderTarget = nullptr;
        std::initializer_list<RenderTargetDrawBuffer> drawBuffers;
        Viewport viewport = { };
        std::vector<Entity*> entities;

        SceneView(const Scene& scene, const Camera& inCamera, RenderTarget* inRenderTarget , std::initializer_list<RenderTargetDrawBuffer>&& inDrawBuffers, Viewport inViewport, u32 flags)
            : camera(inCamera),
            renderTarget(inRenderTarget),
            viewport(inViewport),
            drawBuffers(inDrawBuffers)
        {
            for (auto entity : scene.entities)
            {
                if ((entity->getProperties() & flags) == flags)
                {
                    entities.push_back(entity);
                }
            }
        }
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
    * Encapsulate a mesh draw call, `renderTarget` should be configured to correct state, such as binding color buffers, and clearing
    * color buffers while `drawBuffers` for this draw call will be passed in.
    */
    using RenderSetupLambda = std::function<void(RenderTarget* renderTarget, Shader* shader)>;
    struct RenderTask
    {
        RenderTarget* renderTarget = nullptr;
        std::initializer_list<RenderTargetDrawBuffer> drawBuffers;
        bool clearRenderTarget = true;
        Viewport viewport = { };
        ISubmesh* submesh = nullptr;
        Shader* shader = nullptr;
        GfxPipelineState pipelineState;
        RenderSetupLambda renderSetupLambda = [](RenderTarget* renderTarget, Shader* shader) { };
    };

    class Renderer : public Singleton<Renderer>
    {
    public:
        explicit Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight);
        ~Renderer() {}

        void initialize();
        void finalize();

        GfxContext* getGfxCtx() { return m_ctx; };
        Texture2DRenderable* getColorOutTexture();
        LinearAllocator& getFrameAllocator() { return m_frameAllocator; }

// rendering
        void beginRender();
        void render(Scene* scene, const std::function<void()>& externDebugRender = [](){ });
        void endRender();

        void renderScene(SceneRenderable& renderableScene, const SceneView& sceneView);
        void renderSceneMeshOnly(SceneRenderable& renderableScene, const SceneView& sceneView, Shader* shader);
        void renderSceneDepthNormal(SceneRenderable& renderableScene, const SceneView& sceneView);
        void renderSceneDepthOnly(SceneRenderable& renderableScene, const SceneView& sceneView);
        void renderDebugObjects(Scene* scene, const std::function<void()>& externDebugRender = [](){ });

        // todo: implement the following functions
        void renderShadow(SceneRenderable& sceneRenderable);

        /*
        * Render provided scene into a light probe
        */
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI(const std::function<void()>& callback);

        void drawMeshInstance(SceneRenderable& renderableScene, RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex);

        /**
        * Draw a mesh without material
        */
        void submitMesh(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& renderSetupLambda);

        /**
        * Draw a mesh using same material for all its submesh
        */
        void submitMaterialMesh(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& perMeshSetupLambda);

        /**
        * 
        */
        void submitFullScreenPass(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, Shader* shader, RenderSetupLambda&& renderSetupLambda);

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(RenderTask&& task);

        void setShaderLightingParameters(const SceneRenderable& renderableScene, Shader* shader);
//

// post-processing
        // ssao
        RenderTarget* m_ssaoRenderTarget;
        Texture2DRenderable* m_ssaoTexture;
        Shader* m_ssaoShader;

        void ssao(const SceneView& sceneView, Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture);

        // bloom
        static constexpr u32 kNumBloomPasses = 6u;
        Shader* m_bloomSetupShader;
        Shader* m_bloomDsShader;
        Shader* m_bloomUsShader;
        Shader* m_gaussianBlurShader;
        RenderTarget* m_bloomSetupRenderTarget;
        // bloom chain intermediate buffers
        struct BloomRenderTarget
        {
            RenderTarget* renderTarget;
            Texture2DRenderable* src;
            Texture2DRenderable* scratch;
        };
        BloomRenderTarget m_bloomDsTargets[kNumBloomPasses];
        BloomRenderTarget m_bloomUsTargets[kNumBloomPasses];
        Texture2DRenderable* m_bloomOutTexture;

        void bloom();

        // final composite
        Shader* m_compositeShader;
        Texture2DRenderable* m_compositeColorTexture;
        RenderTarget* m_compositeRenderTarget;
        /*
        * Compositing and resolving to final output color texture. Applying bloom, tone mapping, and gamma correction.
        */
        void composite();
//

// per frame data
        void updateVctxData(Scene* scene);
//
        BoundingBox3D computeSceneAABB(Scene* scene);

        struct Settings
        {
            bool enableAA = true;
            bool enableSunShadow = true;
            bool enableSSAO = true;
            bool enableVctx = false;
            bool enableBloom = true;
            bool autoFilterVoxelGrid = true;
            f32  exposure = 1.f;
            f32 bloomIntensity = 0.7f;
        } m_settings;

        u32           m_windowWidth, m_windowHeight;
        u32           m_SSAAWidth, m_SSAAHeight;

        // normal hdr scene color texture 
        Texture2DRenderable* m_sceneColorTexture;
        Texture2DRenderable* m_sceneNormalTexture;
        Texture2DRenderable* m_sceneDepthTexture;
        RenderTarget* m_sceneColorRenderTarget;

        // hdr super sampling color buffer
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
                        glm::vec4 color;
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
            Texture2DRenderable* irradiance = nullptr;
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
        LinearAllocator m_frameAllocator;
    };
};
