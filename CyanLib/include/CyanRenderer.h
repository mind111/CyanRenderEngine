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
#include "RenderPass.h"
#include "Shadow.h"

extern float quadVerts[24];

namespace Cyan
{
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
        void initRenderTargets(u32 windowWidth, u32 windowHeight);

        StackAllocator& getAllocator();
        RenderTarget* getSceneColorRenderTarget();
        RenderTarget* getRenderOutputRenderTarget();
        Texture* getRenderOutputTexture();
        GfxContext* getGfxCtx() { return m_ctx; };

// shadow
        ShadowmapManager m_shadowmapManager;
        CascadedShadowmap m_csm;
//

// rendering
        void beginRender();
        void render(Scene* scene, const std::function<void()>& debugRender = [](){ });
        void endRender();
        void renderSunShadow(Scene* scene, const std::vector<Entity*>& shaodwCasters);
        void renderScene(Scene* scene);
        void renderSceneDepthNormal(Scene* scene);
        void renderSceneToLightProbe(Scene* scene, Camera& camera);
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
        void ssao(Camera& camera);
        void bloom();
        /*
            * Final compositing and blitting pass that handles tone mapping & gamma correction
        */
        void composite();
//

// per frame data
        void updateFrameGlobalData(Scene* scene, const Camera& camara);
        void updateTransforms(Scene* scene);
        void updateLighting(Scene* scene);
        void updateSunShadow(const CascadedShadowmap& csm);
//
        BoundingBox3f computeSceneAABB(Scene* scene);
        void executeOnEntity(Entity* e, const std::function<void(SceneNode*)>& func);
#if 0
        void addScenePass(Scene* scene);
        // void addDirectionalShadowPass(Scene* scene, const Camera& camera, const std::vector<Entity*>& shadowCasters);
        void addCustomPass(RenderPass* pass);
        void addTexturedQuadPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture);
        void addBloomPass();
        void addGaussianBlurPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture, Texture* horiTexture, Texture* vertTexture, GaussianBlurInput input);
        void addPostProcessPasses();
#endif
        struct Options
        {
            bool enableAA = true;
            bool enableSunShadow = true;
            bool enableSSAO = true;
            bool enableBloom = true;
            f32  exposure = 1.f;
        } m_opts;

        // viewport
        glm::vec2 getViewportSize();
        Viewport getViewport();
        void setViewportSize(glm::vec2 size);
        Viewport m_viewport; 

        // allocators
        StackAllocator m_frameAllocator;

        Uniform* u_model;
        Uniform* u_cameraView;
        Uniform* u_cameraProjection;

        // render targets
        bool          m_bSuperSampleAA;
        u32           m_SSAAWidth, m_SSAAHeight;
        u32           m_offscreenRenderWidth, m_offscreenRenderHeight;
        u32           m_windowWidth, m_windowHeight;
        Shader*       m_sceneDepthNormalShader;

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
        // final render output
        Texture*      m_outputColorTexture;
        RenderTarget* m_outputRenderTarget;

        enum class BufferBindings
        {
            DrawData     = 0,
            DirLightData,
            PointLightsData,
            GlobalTransforms,
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
            kCount = 10
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

        // ssao
        f32           m_ssao;
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
        
        // voxel
        struct VoxelVolumeData
        {
            Texture* m_albedo;
            Texture* m_normal;
            Texture* m_emission;
        };

        u32 m_voxelGridResolution;
        VoxelVolumeData m_voxelData;
        Shader* m_voxelizeShader;
        Shader* m_voxelVisShader;
        MaterialInstance* m_voxelVisMatl;
        MaterialInstance* m_voxelizeMatl;
        RenderTarget* m_voxelRenderTarget;
        RenderTarget* m_voxelVisRenderTarget;
        Texture* m_voxelVisColorTexture;
        Texture* m_voxelColorTexture;
        Texture* m_voxelVolumeTexture;
        Cyan::Texture* voxelizeScene(Scene* scene);
        Cyan::Texture* voxelizeMesh(MeshInstance* mesh, glm::mat4* modelMatrix);
        Cyan::Texture* renderVoxel(Scene* scene);

        static const u32 kNumBloomDsPass = 6u;
        Texture* m_bloomOutput;
        RenderTarget* m_bloomOutputRT;            // final results after upscale chain

        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;

        // for debugging CSM
        Camera m_debugCam;
    private:
        GfxContext* m_ctx;
        static Renderer* m_renderer;
    };
};
