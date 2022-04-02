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

#define gTexBinding(x) static_cast<u32>(GlobalTextureBindings##::##x)
#define gBufferBinding(x) static_cast<u32>(GlobalBufferBindings##::##x)

extern float quadVerts[24];

namespace Cyan
{
    template<typename T>
    struct ShaderStorageBuffer
    {
        std::string blockName;
        GLuint handle = -1;
        GLuint staticBinding = -1;
        T data;

        void bind()
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, staticBinding, handle);
        }

        void updateGfxResource(u32 offset, u32 size)
        {
            glNamedBufferSubData(handle, 0, sizeof(T), &data);
        }
    };

    // foward declarations
    struct RenderTarget;

    class Renderer 
    {
    public:
        explicit Renderer(GLFWwindow* window, const glm::vec2& windowSize);
        ~Renderer() {}

        static Renderer* getSingletonPtr();

        void initialize(GLFWwindow* window, glm::vec2 viewportSize);
        void finalize();

        void initShaders();

        GfxContext* getGfxCtx() { return m_ctx; };
        StackAllocator& getAllocator();
        Texture* getColorOutTexture();
        QuadMesh* getQuadMesh();

// shadow
        ShadowmapManager m_shadowmapManager;
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

        void renderScene(Scene* scene);
        void renderSceneDepthNormal(Scene* scene);
        void renderDebugObjects(const std::function<void()>& externDebugRender = [](){ });

        /*
        * Render provided scene into a light probe
        */
        void renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget);

        /*
        * Render ui widgets given a custom callback defined in an application
        */
        void renderUI(const std::function<void()>& callback);
        void flip();

        void drawEntity(Entity* entity);
        void drawMesh(Mesh* mesh);
        /*
        * Draw a mesh without transform using same type of material for all its submeshes.
        */
        void drawMesh(Mesh* mesh, MaterialInstance* matl, RenderTarget* dstRenderTarget, const std::initializer_list<i32>& drawBuffers, const Viewport& viewport);

        /* 
        * Draw an instanced mesh with transform that allows different types of materials for each submeshes.
        */
        void drawMeshInstance(MeshInstance* meshInstance, i32 transformIndex);
//

// post-processing
        // ssao
        RenderTarget* m_ssaoRenderTarget;
        Texture* m_ssaoTexture;
        Shader* m_ssaoShader;
        MaterialInstance* m_ssaoMatl;
        struct SSAODebugVisData
        {
            glm::vec4 samplePointWS;
            glm::vec4 normal;
            glm::vec4 wo;
            glm::vec4 sliceDir;
            glm::vec4 projectedNormal;
            glm::vec4 sampleVec[16];
            glm::vec4 intermSamplePoints[48];
            int numSampleLines;
            int numSamplePoints;
        } m_ssaoDebugVisData;
        RegularBuffer* m_ssaoDebugVisBuffer;
        bool m_freezeDebugLines;

        struct SSAODebugLines
        {
            ::Line normal;
            ::Line projectedNormal;
            ::Line wo;
            ::Line sliceDir;
            ::Line samples[16];
        } m_ssaoDebugVisLines;
        PointGroup m_ssaoSamplePoints;

        void ssao(Camera& camera);

        // bloom
        static constexpr u32 kNumBloomPasses = 6u;
        Shader* m_bloomSetupShader;
        MaterialInstance* m_bloomSetupMatl;
        Shader* m_bloomDsShader;
        MaterialInstance* m_bloomDsMatl;
        Shader* m_bloomUsShader;
        MaterialInstance* m_bloomUsMatl;
        Shader* m_gaussianBlurShader;
        MaterialInstance* m_gaussianBlurMatl;
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
        MaterialInstance* m_compositeMatl;
        Texture* m_compositeColorTexture;
        RenderTarget* m_compositeRenderTarget;
        /*
        * Compositing and resolving to final output color texture. Applying bloom, tone mapping, and gamma correction.
        */
        void composite();
//

// per frame data
        void updateFrameGlobalData(Scene* scene, const Camera& camara);
        void updateTransforms(Scene* scene);
        void updateLighting(Scene* scene);
        void updateSunShadow(const CascadedShadowmap& csm);
        void updateVctxData(Scene* scene);
//
        BoundingBox3f computeSceneAABB(Scene* scene);
        void executeOnEntity(Entity* e, const std::function<void(SceneNode*)>& func);

        struct Options
        {
            bool enableAA = true;
            bool enableSunShadow = true;
            bool enableSSAO = true;
            bool enableBloom = true;
            bool regenVoxelGridMipmap = true;
            f32  exposure = 1.f;
            f32 bloomIntensity = 0.7f;
        } m_opts;

        // allocators
        StackAllocator m_frameAllocator;

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

        enum class GlobalBufferBindings
        {
            DrawData     = 0,
            DirLightData,
            PointLightsData,
            GlobalTransforms,
            VctxGlobalData,
            kCount
        };

        enum class GlobalTextureBindings
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
            kCount
        };

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
            const u32 kMaxNumTransforms = Scene::kMaxNumSceneNodes;
            const u32 kBufferSize       = kMaxNumTransforms * sizeof(glm::mat4);
            GLuint SBO;
        } gInstanceTransforms;

        // voxel cone tracing
        // todo: fix number of mipmaps, something like 5 should be enough
        struct VoxelGrid
        {
            const u32 resolution = 128u;
            // const u32 maxNumMips = 4u;
            Texture* albedo;
            Texture* normal;
            Texture* emission;
            Texture* radiance;
            glm::vec3 localOrigin;
            f32 voxelSize;
        } m_sceneVoxelGrid;

        struct VctxVis
        {
            enum class Mode
            {
                kAlbedo = 0,
                kOpacity,
                kRadiance,
                kNormal,
                kCount
            } mode = Mode::kAlbedo;

            // help visualize a traced cone
            f32 boost = 1.f;
            glm::vec3 debugRayOrigin = glm::vec3(-4.f, 2.f, 0.f);
            glm::vec3 debugRayDir = glm::vec3(-1.f, 0.f, 0.f);
            glm::vec2 debugScreenPos = glm::vec2(0.f);

            i32 activeMip = 0u;

            GLuint ssbo;
            GLuint idbo;
            const static i32 ssboBinding = (i32)GlobalBufferBindings::kCount;
            const static u32 kMaxNumCubes = 1024u;

            Shader* coneVisDrawShader;
            Shader* coneVisComputeShader;
            Shader* vctxVisShader;

            RenderTarget* renderTarget;

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
                ConeCube cubes[kMaxNumCubes];
            } debugConeBuffer;

        } m_vctxVis;

        struct VctxGpuData
        {
            glm::vec3 localOrigin;
            f32 voxelSize;
            u32 visMode;
            glm::vec3 padding;
        } m_vctxGpuData;
        GLuint m_vctxSsbo;

        const char* vctxVisModeNames[(u32)VctxVis::Mode::kCount] = { 
            "albedo",
            "opacity",
            "radiance",
            "normal",
        };

        Shader* m_voxelizeShader;
        MaterialInstance* m_voxelizeMatl;
        Shader* m_voxelVisShader;
        MaterialInstance* m_voxelVisMatl;
        RenderTarget* m_voxelizeRenderTarget;
        Texture* m_voxelizeColorTexture;
        RenderTarget* m_voxelVisRenderTarget;
        Texture* m_voxelVisColorTexture;
        Shader* m_filterVoxelGridShader;

        /*
        * Voxelize a given scene
        */
        void voxelizeScene(Scene* scene);

        /*
        * Visualization to help debug voxel cone tracing
        */
        void visualizeVoxelGrid();
        void visualizeVctxAo();
        void visualizeConeTrace();

        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;

        // for debugging CSM
        Camera m_debugCam;
    private:
        GfxContext* m_ctx;
        static Renderer* m_renderer;
    };
};
