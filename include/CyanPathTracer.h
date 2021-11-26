#pragma once
#include "scene.h"
namespace Cyan 
{
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);

    class PathTracer
    {
    public:
        PathTracer(Scene* scene);

        // inputs: scene, camera
        void progressiveRender(Scene* scene, Camera& camera);
        void render(Scene* scene, Camera& camera);

        void progressiveIndirectLighting();

        glm::vec3 computeDirectLighting();
        glm::vec3 computeDirectSkyLight(glm::vec3& ro, glm::vec3& n);
        glm::vec3 traceScene(glm::vec3& ro, glm::vec3& rd);        
        f32 traceShadowRay(glm::vec3& ro, glm::vec3& rd);
        void setPixel(u32 px, u32 py, glm::vec3& color);

        // global illumination
        glm::vec3 recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces);

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 2u;
        const u32 sppyCount = 2u;
        const u32 numChannelPerPixel = 3u;
        const u32 perFrameWorkGroupX = 4u;
        const u32 perFrameWorkGroupY = 4u;
        const u32 kSpp = 32;

        // about 2.6 MB
        glm::vec3 hitPositionCache[640][360];
        u32       numAccumulatedSamples;
        Camera     m_staticCamera;
        u32        m_numTracedPixels; 
        glm::uvec2 m_checkPoint;
        Texture* m_texture;
        Scene* m_scene;
        float* m_pixels;
    };
}