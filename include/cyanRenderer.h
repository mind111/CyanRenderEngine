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
        void renderScene(Scene* scene);
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

        u32 m_renderWidth, m_renderHeight;
        Texture* m_defaultColorBuffer; // hdr
        RenderTarget* m_defaultRenderTarget;
        Texture* m_msaaColorBuffer;
        RenderTarget* m_msaaRenderTarget;
    };
}
