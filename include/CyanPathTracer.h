#pragma once
#include "scene.h"
namespace Cyan 
{
    class PathTracer
    {
    public:
        PathTracer(Scene* scene);

        // inputs: scene, camera
        void progressiveRender(Scene* scene, Camera& camera);
        void render(Scene* scene, Camera& camera);
        glm::vec3 traceScene(glm::vec3& ro, glm::vec3& rd);        
        f32 traceShadowRay(glm::vec3& ro, glm::vec3& rd);
        void setPixel(u32 px, u32 py, glm::vec3& color);

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 numChannelPerPixel = 3u;
        const u32 perFrameWorkGroupX = 4u;
        const u32 perFrameWorkGroupY = 4u;
        const u32 kSpp = 32;

        Camera     m_staticCamera;
        u32        m_numTracedRays; 
        glm::uvec2 m_checkPoint;
        Texture* m_texture;
        Scene* m_scene;
        float* m_pixels;
    };
}