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

    struct IrradianceVolume
    {

    };

    struct IrradianceProbe
    {
        glm::vec3 m_position;
        // render the scene from six faces
        Texture* m_radianceSamples;
        // convolve the radiance samples to get irradiance from every direction
        Texture* m_irradianceMap;
        Scene* m_scene;
        MeshInstance* m_meshInstance;
        static Shader* m_renderProbeShader;
        static MaterialInstance* m_renderProbeMatl;
        static RenderTarget* m_radianceRenderTarget;
        static RenderTarget* m_irradianceRenderTarget;

        void init();

        void sampleRadiance();
        void computeIrradiance();
        void debugRenderProbe();
    };

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
        void renderScene(Scene* scene, Camera& camera);
        void endRender();

        struct Lighting
        {
            std::vector<PointLightData>* m_pLights;
            std::vector<DirLightData>* m_dirLights;
            LightProbe* m_probe;
            bool bUpdateProbeData;
        };

        void drawEntity(Entity* entity);
        void drawSceneNode(SceneNode* node);
        void drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix);
        // blit viewport to default frame buffer for debug rendering
        void debugBlit(Cyan::Texture* texture, Viewport viewport);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();

        void addScenePass(Scene* scene);
        void addDirectionalShadowPass();
        void addCustomPass(RenderPass* pass);
        void addTexturedQuadPass(RenderTarget* renderTarget, Viewport viewport, Texture* srcTexture);
        void addBloomPass();
        void addGaussianBlurPass();
        void addLinePass();
        void addPostProcessPasses();

        Lighting m_lighting;
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
        RenderTarget* m_sceneColorRenderTarget;
        // hdr super sampling color buffer
        Texture* m_sceneColorTextureSSAA;
        RenderTarget* m_sceneColorRTSSAA;
        // final render output
        Texture* m_outputColorTexture;
        RenderTarget* m_outputRenderTarget;
        
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
    private:
        static Renderer* m_renderer;
    };
};
