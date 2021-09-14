#pragma once

#include <functional>
#include <map>

#include "glew.h"
#include "stb_image.h"

#include "Common.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "camera.h"
#include "Entity.h"
#include "Geometry.h"

extern float quadVerts[24];

namespace Cyan
{
    // foward declarations
    struct RenderTarget;

    struct RenderPass
    {
        RenderTarget* m_renderTarget;
        Viewport m_viewport;
        virtual void render();
    };

    struct ScenePass : public RenderPass
    {
        Scene* m_scene;
        virtual void render() override;
    }
    
    struct MeshPass : public RenderPass
    {
        MeshInstance* m_mesh;
        MaterialInstance* m_matl;
        glm::mat4 m_modelMatrix;
    };
    
    struct TexturedQuadPass : public RenderPass
    {
        Texture* m_srcTexture;
        virtual void render() override;
    };

    struct LinePass : public RenderPass
    {
        // mesh
        virtual void render() override;
    };

    class RenderState
    {
        bool m_superSampleAA; 
        bool m_bloom;
        std::vector<RenderPass*> m_renderPasses;
    };

    class Renderer 
    {
    public:
        Renderer() {}
        ~Renderer() {}

        static Renderer* get();

        void init(glm::vec2 viewportSize);
        void initRenderTargets(u32 windowWidth, u32 windowHeight);
        void initShaders();

        void beginRender();
        void renderScene(Scene* scene);
        void endRender();

        struct Lighting
        {
            std::vector<PointLight>* m_pLights;
            std::vector<DirectionalLight>* m_dirLights;
            LightProbe* m_probe;
            bool bUpdateProbeData;
        };

        Lighting m_lighting;
        void drawEntity(Entity* entity);
        void drawSceneNode(SceneNode* node);
        void drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix);
        // blit viewport to default frame buffer for debug rendering
        void debugBlit(Cyan::Texture* texture, Viewport viewport);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();
        RenderState m_renderState;
        void addScenePass(Scene* scene, Viewport viewport)
        {
            ScenePass pass = {0};
            pass.m_scene = scene;
            pass.m_viewport = viewport;
            pass.m_renderTarget = m_renderState.m_superSampleAA ? m_superSamplingRenderTarget : m_defaultRenderTarget;
            m_renderState.m_renderPasses.push_back(pass);
        }
        void addTexturedQuadPass()
        {

        }
        void addBloomPass()
        {

        }
        void addGaussianBlurPass()
        {

        }
        void addLinePass()
        {

        }

        Frame* m_frame;

        // viewport
        glm::vec2 getViewportSize();
        Viewport getViewport();
        void setViewportSize(glm::vec2 size);
        // viewport's x,y are in framebuffer's space
        Viewport m_viewport; 

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
        u32 m_superSamplingRenderWidth, m_superSamplingRenderHeight;
        u32 m_offscreenRenderWidth, m_offscreenRenderHeight;
        u32 m_windowWidth, m_windowHeight;

        Texture* m_defaultColorBuffer;             // hdr
        RenderTarget* m_defaultRenderTarget;
        Texture* m_superSamplingColorBuffer;       // hdr
        RenderTarget* m_superSamplingRenderTarget;
        Texture* m_msaaColorBuffer;
        RenderTarget* m_msaaRenderTarget;
        
        // voxel
        Cyan::Texture* voxelizeMesh(MeshInstance* mesh, glm::mat4* modelMatrix);

        struct BloomSurface
        {
            RenderTarget* m_renderTarget;
            Texture* m_pingPongColorBuffers[2];
        };

        static const u32 kNumBloomDsPass = 6u;
        RenderTarget* m_bloomPrefilterRT;                         // preprocess surface
        // TODO: Is this necessary?
        RenderTarget* m_bloomResultRT;                            // final results after upscale chain
        BloomSurface m_bloomDsSurfaces[kNumBloomDsPass];          // downsample
        BloomSurface m_bloomUsSurfaces[kNumBloomDsPass];          // upsample

        // post-process
        void beginBloom();
        void bloomDownSample();
        void bloomUpScale();
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

        void gaussianBlur(BloomSurface surface, GaussianBlurInputs inputs);
        void bloom();
        void blitPass();

        // post processing params
        bool m_bloom;
        f32  m_exposure;
        u32  m_numBloomTextures;
        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;
    };
};
