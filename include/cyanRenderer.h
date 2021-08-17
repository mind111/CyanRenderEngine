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
    struct DrawCall
    {
        u32 m_index;
        Mesh* m_mesh;
        u32 m_uniformBegin;
        u32 m_uniformEnd;
        glm::mat4 m_modelTransform;
    };

    struct Frame
    {
        std::vector<DrawCall> m_renderQueue;
    };
    
    // foward declarations
    struct RenderTarget;

    class Renderer 
    {
    public:
        Renderer() {}
        ~Renderer() {}

        static Renderer* get();

        void init(u32 renderWidth, u32 renderHeight);
        void initRenderTargets(u32 windowWidth, u32 windowHeight);
        void initShaders();

        void beginRender();
        void renderScene(Scene* scene);
        void endRender();

        void drawEntity(Entity* entity);
        void drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();

        struct BloomSurface
        {
            RenderTarget* m_renderTarget;
            Texture* m_pingPongColorBuffers[2];
        };

        Frame* m_frame;

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

        static const u32 kNumBloomDsPass = 7u;
        RenderTarget* m_bloomPrefilterRT;                         // preprocess surface
        BloomSurface m_bloomDsSurfaces[kNumBloomDsPass];          // downsample
        BloomSurface m_bloomUsSurfaces[kNumBloomDsPass];          // upsample

        // post-process
        void beginBloom();
        void downSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx);
        void upSample(RenderTarget* src, u32 srcIdx, RenderTarget* dst, u32 dstIdx, RenderTarget* blend, u32 blendIdx);
        void gaussianBlur(BloomSurface surface);
        void bloom();

        // post processing params
        bool m_bloom;
        f32  m_exposure;
        u32  m_numBloomTextures;
        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;
    };
}
