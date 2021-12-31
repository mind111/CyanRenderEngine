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

extern float quadVerts[24];

namespace Cyan
{
    class FrameListener
    {
        FrameListener() { }
        ~FrameListener() { }
        virtual void onFrameStart();
        virtual void onFrameEnd();
    };
    
    // TODO: implement this
    struct RenderView
    {
        RenderTarget* m_renderTarget;
        Viewport m_viewport;
        glm::vec4 m_clearColor;
    };

    // foward declarations
    struct RenderTarget;

    class RenderState
    {
    public:
        bool m_superSampleAA; 
        bool m_bloom;
        std::vector<RenderPass*> m_renderPasses;
        // render targets that need to be cleared from last frame
        std::vector<RenderTarget*> m_clearRenderTargetList;

        void addRenderPass(RenderPass* pass)
        {
            m_renderPasses.push_back(pass);
        }

        void addClearRenderTarget(RenderTarget* renderTarget)
        {
            m_clearRenderTargetList.push_back(renderTarget);
        }

        void clearRenderTargets()
        {
            for (auto renderTarget : m_clearRenderTargetList)
            {
                auto ctx = getCurrentGfxCtx();
                ctx->setRenderTarget(renderTarget, 0u);
                // clear all the color buffers
                std::vector<i32> drawBuffers;
                for (u32 b = 0u; b < renderTarget->m_colorBuffers.size(); ++b)
                {
                    drawBuffers.push_back(b);
                }
                ctx->setRenderTarget(renderTarget, drawBuffers.data(), static_cast<u32>(drawBuffers.size()));
                ctx->clear();
            }
            m_clearRenderTargetList.clear();
        }

        void clearRenderPasses()
        {
            m_renderPasses.clear();
        }
    };

    class Renderer 
    {
    public:
        explicit Renderer();
        ~Renderer() {}

        static Renderer* getSingletonPtr();

        void init(glm::vec2 viewportSize);
        void initRenderTargets(u32 windowWidth, u32 windowHeight);
        void initShaders();

        StackAllocator& getAllocator();
        RenderTarget* getSceneColorRenderTarget();
        RenderTarget* getRenderOutputRenderTarget();
        Texture* getRenderOutputTexture();

        void beginRender();
        void render();
        void renderSceneDepthNormal(Scene* scene, Camera& camera);
        void probeRenderScene(Scene* scene, Camera& camera);
        void renderSSAO(Camera& camera);
        void renderScene(Scene* scene, Camera& camera);
        void renderDebugObjects();
        void endRender();

        // gpu compatible lighting data
        struct LightingGpuData
        {
            RegularBuffer* pointLightsBuffer;
            RegularBuffer* dirLightsBuffer;
            std::vector<PointLightGpuData> pLights;
            std::vector<DirLightGpuData> dLights;
            DistantLightProbe* probe;
            IrradianceProbe* irradianceProbe;
            ReflectionProbe* reflectionProbe;
            bool bUpdateProbe;
        } m_gpuLightingData;

        void uploadGpuLightingData(LightingEnvironment& lighting);
        void renderEntities(std::vector<Entity*>& entities, LightingEnvironment& lighting, Camera& camera);
        BoundingBox3f computeSceneAABB(Scene* scene);
        void drawEntity(Entity* entity);
        void drawSceneNode(SceneNode* node);
        void drawMesh(Mesh* mesh);
        void drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix);
        // blit viewport to default frame buffer for debug rendering
        void debugBlit(Cyan::Texture* texture, Viewport viewport);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();

        void addScenePass(Scene* scene);
        void addEntityPass(RenderTarget* renderTarget, Viewport viewport, std::vector<Entity*>& entities, LightingEnvironment& lighting, Camera& camera);
        void addDirectionalShadowPass(Scene* scene, Camera& camera, u32 lightIndex);
        void addCustomPass(RenderPass* pass);
        void addTexturedQuadPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture);
        void addBloomPass();
        void addGaussianBlurPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture, Texture* horiTexture, Texture* vertTexture, GaussianBlurInput input);
        void addLinePass();
        void addPostProcessPasses();

        RenderState m_renderState;

        // viewport
        glm::vec2 getViewportSize();
        Viewport getViewport();
        void setViewportSize(glm::vec2 size);
        // viewport's x,y are in framebuffer's space
        Viewport m_viewport; 

        // allocators
        StackAllocator m_frameAllocator;

        Uniform* u_model;
        Uniform* u_cameraView;
        Uniform* u_cameraProjection;

        RegularBuffer* m_pointLightsBuffer;
        RegularBuffer* m_dirLightsBuffer;

        // render targets
        bool m_bSuperSampleAA;
        // u32 m_superSamplingRenderWidth, m_superSamplingRenderHeight;
        u32 m_SSAAWidth, m_SSAAHeight;
        u32 m_offscreenRenderWidth, m_offscreenRenderHeight;
        u32 m_windowWidth, m_windowHeight;

        // normal hdr scene color texture 
        Texture* m_sceneColorTexture;
        Texture* m_sceneNormalTexture;
        Texture* m_sceneDepthTexture;
        RenderTarget* m_sceneColorRenderTarget;
        // hdr super sampling color buffer
        Texture* m_sceneColorTextureSSAA;
        Texture* m_sceneNormalTextureSSAA;
        RenderTarget* m_sceneColorRTSSAA;
        Texture* m_sceneDepthTextureSSAA;
        Shader* m_sceneDepthNormalShader;
        // TODO: whether enabling post-processing or not, the final render output should always be in 
        // following render targets
        // final render output
        Texture* m_outputColorTexture;
        RenderTarget* m_outputRenderTarget;

        // directional shadow
        CascadedShadowMap m_cascadedShadowMap;
        const u32 kNumShadowCascades = 4u;
        Shader* m_directionalShadowShader;
        MaterialInstance* m_directionalShadowMatl;
        RenderTarget* m_depthRenderTarget;

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

        // post processing params
        bool m_bloom;
        f32  m_exposure;
        u32  m_numBloomTextures;
        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;

        // for debugging CSM
        Camera m_debugCam;
    private:
        static Renderer* m_renderer;
    };
};