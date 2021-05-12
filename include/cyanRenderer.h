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

        void init();
        void render(Scene* scene);
        void drawEntity(Entity* entity);
        void drawMesh(Mesh* _mesh);
        void submitMesh(Mesh* mesh, glm::mat4 modelTransform);
        void renderFrame();
        void endFrame();

        Frame* m_frame;
        // Per entity xform uniform
        Uniform* u_model; 
    };
}
