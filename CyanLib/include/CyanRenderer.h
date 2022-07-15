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
    struct RenderableScene;

    struct SceneView
    {
        ICamera* camera = nullptr;
        std::vector<Entity*> entities;

        SceneView(const Scene& scene, ICamera* inCamera, u32 flags)
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

    struct RenderQueue;

    /**
    * Proxy to concrete rendering resources such as Textures, Buffers. This allows deferred resource allocations
    */
    struct RenderResource
    {
        RenderResource(const char* inTag)
            : tag(inTag)
        {}
        virtual ~RenderResource() { }

        virtual void createResource(RenderQueue& renderQueue) = 0;
        void addRef() { refCount++; }
        void removeRef() 
        { 
            refCount--; 
            if (refCount == 0)
            {

            }
        }
        const char* tag = nullptr;
        u32 refCount = 0;
    };

    struct RenderTexture2D : public RenderResource
    {
        RenderTexture2D(const char* inTag, const ITextureRenderable::Spec& inSpec)
            : RenderResource(inTag), spec(inSpec)
        { }

        virtual void createResource(RenderQueue& renderQueue) override;

        ITextureRenderable::Spec spec = { };
        Texture2DRenderable* texture = nullptr;
        Texture2DRenderable* getTexture() { return texture; }
    };

    struct RenderPass
    {
        RenderPass(const char* inTag)
            : tag(inTag)
        { }

        void addInput(RenderResource* resource) 
        { 
            resource->addRef();
            inputs.push_back(resource);
        }

        void addOutput(RenderResource* resource) 
        { 
            outputs.push_back(resource);
        }

        const char* tag = nullptr;
        std::vector<RenderResource*> inputs;
        std::vector<RenderResource*> outputs;
        std::function<void()> lambda = []() { };
    };

    /**
    * render queue management
    * todo: eventually convert this to a render graph
    */
    struct RenderQueue
    {
        // transient resource management
        struct TransientResourceManager
        {
            Texture2DRenderable* createTexture2D(const char* name, const ITextureRenderable::Spec& inSpec) 
            { 
                Texture2DRenderable* outTexture = nullptr;
                i32 index = -1;
                // find reusable
                for (u32 i = 0; i < reusableTextures.size(); ++i)
                {
                    // check if the specs are compatible
                    if (reusableTextures[i]->getTextureSpec() == inSpec)
                    {
                        outTexture = static_cast<Texture2DRenderable*>(reusableTextures[i]);
                        index = i;
                        break;
                    }
                }
                if (outTexture)
                {
                    reusableTextures.erase(reusableTextures.begin() + index);
                }
                else 
                {
                    // if not create new resources
                    outTexture = createTexture2DTransient(name, inSpec);
                }
                return outTexture;
            };

            /**
            */
            Texture2DRenderable* createTexture2DTransient(const char* name, const ITextureRenderable::Spec& inSpec)
            {
                Texture2DRenderable* outTexture = new Texture2DRenderable(name, inSpec);
                // transientTextures.insert({ std::string(name), outTexture });
                textures.push_back(outTexture);
                return outTexture;
            }

            void recycle(RenderResource* resource)
            {
                if (auto renderTexture2D = dynamic_cast<RenderTexture2D*>(resource))
                    reusableTextures.push_back(renderTexture2D->getTexture());
            }

            std::vector<ITextureRenderable*> reusableTextures;

            std::vector<ITextureRenderable*> textures;
            std::unordered_map<std::string, ITextureRenderable*> transientTextures;
        } transientResourceManager;

        RenderQueue()
        {
            allocator = std::make_unique<LinearAllocator>(1024 * 1024 * 128);
        }

        /**
        * create resources proxies
        */
        RenderTexture2D* createTexture2D(const char* name, const ITextureRenderable::Spec& inSpec) 
        { 
            return allocator->alloc<RenderTexture2D>(name, inSpec);
        }

        /**
        * create concrete gpu resources
        */
        Texture2DRenderable* createTexture2DInternal(const char* name, const ITextureRenderable::Spec& inSpec) 
        { 
            Texture2DRenderable* outTexture = transientResourceManager.createTexture2D(name, inSpec);
            return outTexture;
        }

        void addPass(const char* passName, const std::function<void(RenderQueue&, RenderPass*)>& setupLambda, const std::function<void()>& executeLambda)
        {
            RenderPass* pass = allocator->alloc<RenderPass>(passName);
            // setup pass input/output
            setupLambda(*this, pass);
            pass->lambda = executeLambda;
            renderPassQueue.push(pass);
        }

        void compile() { }
        void execute() 
        { 
            while (!renderPassQueue.empty())
            {
                auto pass = renderPassQueue.front();
                /** before execute create concrete gpu resources such as intermediate/output 
                * textures and buffers
                */
                for (auto output : pass->outputs)
                {
                    output->createResource(*this);
                }

                // execute
                pass->lambda();

                // after execute
                for (auto input : pass->inputs)
                {
                    input->removeRef();

                    // recycle reusable resources
                    if (input->refCount == 0)
                    {
                        transientResourceManager.recycle(input);
                    }
                }

                // explicit invoke destructor to avoid leaking resources
                pass->~RenderPass();
                renderPassQueue.pop();
            }

            allocator->reset();
        }

    private:
        std::unique_ptr<LinearAllocator> allocator = nullptr;
        std::queue<RenderPass*> renderPassQueue;
        std::unordered_map<std::string, RenderPass*> renderPassMap;
    };

    class Renderer : public Singleton<Renderer>
    {
    public:
        using UIRenderCommand = std::function<void()>;

        explicit Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight);
        ~Renderer() {}

        void initialize();
        void finalize();

        GfxContext* getGfxCtx() { return m_ctx; };
        LinearAllocator& getFrameAllocator() { return m_frameAllocator; }

// rendering
        void beginRender();
        void render(Scene* scene, const std::function<void()>& externDebugRender = [](){ });
        void renderToScreen(RenderTexture2D* inTexture);
        void endRender();

        RenderTexture2D* renderScene(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution);
        void renderSceneMeshOnly(RenderableScene& renderableScene, const SceneView& sceneView, Shader* shader);

        struct ScenePrepassOutput
        {
            RenderTexture2D* depthTexture;
            RenderTexture2D* normalTexture;
        };

        ScenePrepassOutput renderSceneDepthNormal(RenderableScene& renderableScene, const SceneView& sceneView, const glm::uvec2& outputResolution);
        void renderSceneDepthOnly(RenderableScene& renderableScene, const SceneView& sceneView);
        void renderDebugObjects(Scene* scene, const std::function<void()>& externDebugRender = [](){ });

        void renderShadow(RenderableScene& sceneRenderable);

        /*
        * Render provided scene into a light probe
        */
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);

        void addUIRenderCommand(const std::function<void()>& UIRenderCommand);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI();

        void drawMeshInstance(RenderableScene& renderableScene, RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers, bool clearRenderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex);

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
//

// post-processing
        // gaussian blur
        RenderTexture2D* gaussianBlur(RenderTexture2D* inTexture, u32 inRadius, f32 inSigma);

        // taa
        glm::vec2 TAAJitterVectors[16] = { };
        f32 reconstructionWeights[16] = { };
        glm::mat4 m_originalProjection = glm::mat4(1.f);
        Texture2DRenderable* m_TAAOutput = nullptr;
        RenderTarget* m_TAAPingPongRenderTarget[2] = { 0 };
        Texture2DRenderable* m_TAAPingPongTextures[2] = { 0 };
        void taa();
        void ssao(const SceneView& sceneView, RenderTexture2D* sceneDepthTexture, RenderTexture2D* sceneNormalTexture, const glm::uvec2& outputResolution);

        struct DownsampleChain
        {
            std::vector<RenderTexture2D*> stages;
            u32 numStages;
        };
        DownsampleChain downsample(RenderTexture2D* inTexture, u32 inNumStages);
        RenderTexture2D* bloom(RenderTexture2D* inTexture);

        /**
        * Local tonemapping using "Exposure Fusion"
        * https://bartwronski.com/2022/02/28/exposure-fusion-local-tonemapping-for-real-time-rendering/
        * https://mericam.github.io/papers/exposure_fusion_reduced.pdf 
        */
        void localToneMapping();

        /*
        * Compositing and resolving to final output color texture. Applying bloom, tone mapping, and gamma correction.
        */
        RenderTexture2D* composite(RenderTexture2D* inSceneColor, RenderTexture2D* inBloomColor, const glm::uvec2& outputResolution);
//

// per frame data
        void updateVctxData(Scene* scene);
//
        BoundingBox3D computeSceneAABB(Scene* scene);

        struct Settings
        {
            bool enableAA = true;
            bool enableTAA = false;
            bool enableSunShadow = true;
            bool enableSSAO = false;
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
        u32 m_numFrames = 0u;
        LinearAllocator m_frameAllocator;
        std::queue<UIRenderCommand> m_UIRenderCommandQueue;
        RenderQueue m_renderQueue;
    };
};
