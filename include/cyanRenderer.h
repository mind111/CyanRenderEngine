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

class aiMesh;
class Renderer;

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

    class Renderer 
    {
    public:
        Renderer() {}
        ~Renderer() {}

        static Renderer* get();

        void init(u32 renderWidth, u32 renderHeight);

        void beginRender();
        void renderScene(Scene* scene);
        void endRender();

        void drawEntity(Entity* entity);
        void drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();

        Frame* m_frame;
        // Per entity xform uniform
        Uniform* u_model; 
        Uniform* u_cameraView;
        Uniform* u_cameraProjection;

        RegularBuffer* m_pointLightsBuffer;
        RegularBuffer* m_dirLightsBuffer;

        // shaders
        Shader* m_blitShader;
        MaterialInstance* m_blitMaterial;

        struct BlitQuadMesh
        {
            VertexBuffer* m_vb;
            VertexArray*  m_va;
            MaterialInstance* m_matl;
        };

        // blit quad mesh
        BlitQuadMesh* m_blitQuad;
        // Quad m_blitQuad;

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

        // post processing params
        f32  m_exposure;
        GLuint m_lumHistogramShader;
        GLuint m_lumHistogramProgram;
    };
}
