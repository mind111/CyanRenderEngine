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
    struct SceneView
    {
        Camera camera = { };
        RenderTarget* renderTarget = nullptr;
        std::initializer_list<i32> drawBuffers;
        Viewport viewport = { };
        std::vector<Entity*> entities;

        SceneView(Scene* scene, const Camera& inCamera, RenderTarget* inRenderTarget , std::initializer_list<i32>&& inDrawBuffers, Viewport inViewport, u32 flags)
            : camera(inCamera),
            renderTarget(inRenderTarget),
            viewport(inViewport),
            drawBuffers(inDrawBuffers)
        {
            for (auto entity : scene->entities)
            {
                if ((entity->getFlags() & flags) == flags)
                {
                    entities.push_back(entity);
                }
            }
        }
    };

    enum class SceneBufferBindings
    {
        kViewData = 0,
        DirLightData,
        PointLightsData,
        TransformMatrices,
        VctxGlobalData,
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
        std::initializer_list<i32> drawBuffers;
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
        Texture* getColorOutTexture();

// shadow
        std::unique_ptr<RasterDirectShadowManager> m_rasterDirectShadowManager;
        CascadedShadowmap m_csm;
//

// rendering
        void beginRender();
        void render(Scene* scene, const std::function<void()>& externDebugRender = [](){ });
        void endRender();

        /*
        * TODO: min/max depth for each cascade to help shrink the orthographic projection size
        * TODO: blend between cascades to alleviate artifact when transitioning between cascades
        * TODO: better shadow biasing; normal bias and receiver geometry bias
        * TODO: distribution based shadow mapping methods: VSM, ESM etc.
        * 
        * Render a directional shadow map for the sun in 'scene'
        */
        void renderSunShadow(Scene* scene, const std::vector<Entity*>& shaodwCasters);
        void renderScene(Scene* scene, const SceneView& sceneView);
        void renderSceneDepthNormal(Scene* scene, const SceneView& sceneView);
        void renderDebugObjects(Scene* scene, const std::function<void()>& externDebugRender = [](){ });
        /*
        * Render provided scene into a light probe
        */
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI(const std::function<void()>& callback);

        void drawEntity(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Entity* entity);
        void drawMeshInstance(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex);

        /**
        * Draw a mesh without material
        */
        void submitMesh(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& renderSetupLambda);

        /**
        * Draw a mesh using same material for all its submesh
        */
        void submitMaterialMesh(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, IMaterial* material, const RenderSetupLambda& renderSetupLambda);

        /**
        * 
        */
        void submitFullScreenPass(RenderTarget* renderTarget, std::initializer_list<i32>&& drawBuffers, Shader* shader, const RenderSetupLambda& renderSetupLambda);

        /**
        * Submit a submesh; right now the execution is not deferred
        */
        void submitRenderTask(RenderTask&& task);
//

// post-processing
        // ssao
        RenderTarget* m_ssaoRenderTarget;
        Texture* m_ssaoTexture;
        Shader* m_ssaoShader;

        void ssao(Camera& camera);

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
            Texture* src;
            Texture* scratch;
        };
        BloomRenderTarget m_bloomDsTargets[kNumBloomPasses];
        BloomRenderTarget m_bloomUsTargets[kNumBloomPasses];
        Texture* m_bloomOutTexture;

        void bloom();

        // final composite
        Shader* m_compositeShader;
        Texture* m_compositeColorTexture;
        RenderTarget* m_compositeRenderTarget;
        /*
        * Compositing and resolving to final output color texture. Applying bloom, tone mapping, and gamma correction.
        */
        void composite();
//

// per frame data
        void updateSunShadow(const CascadedShadowmap& csm);
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
        Texture*      m_sceneColorTexture;
        Texture*      m_sceneNormalTexture;
        Texture*      m_sceneDepthTexture;
        RenderTarget* m_sceneColorRenderTarget;

        // hdr super sampling color buffer
        Texture*      m_sceneColorTextureSSAA;
        Texture*      m_sceneNormalTextureSSAA;
        RenderTarget* m_sceneColorRTSSAA;
        Texture*      m_sceneDepthTextureSSAA;
        Shader*       m_sceneDepthNormalShader;

        Texture*      m_finalColorTexture;

#if 0
        struct GlobalDrawData
        {
            glm::mat4 view;
            glm::mat4 projection;
            glm::mat4 sunLightView;
            glm::mat4 sunShadowProjections[4];
            i32       numDirLights;
            i32       numPointLights;
            f32       ssao;
            f32       dummy;
        } m_globalDrawData;
        GLuint gDrawDataBuffer;

        struct Lighting
        {
            const u32                      kDynamicLightBufferSize = 1024;
            CascadedShadowmap              sunLightShadowmap;
            std::vector<PointLightGpuData> pointLights;
            std::vector<DirLightGpuData>   dirLights;
            GLuint                         dirLightSBO;
            GLuint                         pointLightsSBO;
            Skybox*                        skybox;
            IrradianceProbe*               irradianceProbe;
            ReflectionProbe*               reflectionProbe;
        } gLighting;

        struct GlobalTransforms
        {
            const u32 kMaxNumTransforms = Scene::kMaxNumSceneComponents;
            const u32 kBufferSize       = kMaxNumTransforms * sizeof(glm::mat4);
            GLuint sbo;
        } gInstanceTransforms;
#endif

        // voxel cone tracing
        struct VoxelGrid
        {
            // todo: for some reason settings this to 128 crashes RenderDoc every time
            const u32 resolution = 128u;
            // const u32 resolution = 8u;
            Texture* albedo = nullptr;
            Texture* normal = nullptr;
            Texture* emission = nullptr;
            Texture* radiance = nullptr;
            Texture* opacity = nullptr;
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
                Texture* colorBuffer = nullptr;
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
                Texture* cachedSceneDepth = nullptr;
                Texture* cachedSceneNormal = nullptr;

                Shader* voxelGridVisShader = nullptr;
                RenderTarget* renderTarget = nullptr;
                Texture* colorBuffer = nullptr;

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
            Texture* occlusion = nullptr;
            Texture* irradiance = nullptr;
            Texture* reflection = nullptr;
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
    };
};
