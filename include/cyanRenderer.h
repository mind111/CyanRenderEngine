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
        void renderScene(Scene* scene);
        void endRender();

        struct Lighting
        {
            std::vector<PointLight>* m_pLights;
            std::vector<DirectionalLight>* m_dirLights;
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

        // shaders
        Shader* m_blitShader;
        Shader* m_bloomPreprocessShader;
        MaterialInstance* m_bloomPreprocessMatl;
        Shader* m_gaussianBlurShader;
        MaterialInstance* m_gaussianBlurMatl;
        MaterialInstance* m_blitMaterial;

        struct BlitQuadMesh
        {
            VertexBuffer* m_vb;
            VertexArray*  m_va;
            MaterialInstance* m_matl;
        };

        // blit quad mesh
        BlitQuadMesh* m_blitQuad;

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
        Shader* m_voxelizeShader;
        MaterialInstance* m_voxelizeMatl;
        RenderTarget* m_voxelRenderTarget;
        Texture* m_voxelColorTexture;
        Cyan::Texture* voxelizeMesh(MeshInstance* mesh, glm::mat4* modelMatrix);

        struct BloomSurface
        {
            RenderTarget* m_renderTarget;
            Texture* m_pingPongColorBuffers[2];
        };

        static const u32 kNumBloomDsPass = 6u;
        Texture* m_bloomOutput;
        RenderTarget* m_bloomOutputRT;            // final results after upscale chain

        // post-process
        // void beginBloom();
        // void bloomDownSample();
        // void bloomUpScale();
        void downSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx);

        struct UpScaleInputs
        {
            i32 stageIndex;
        };
        void upSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx, RenderTarget* blend, u32 blendIdx, UpScaleInputs inputs);

        struct GaussianBlurInputs
        {
            i32 kernelIndex;
            i32 radius;
        };

        void gaussianBlur(BloomPass::BloomSurface surface, GaussianBlurInputs inputs);
        // void bloom();
        // void blitPass();

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
