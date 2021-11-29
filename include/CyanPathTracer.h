#pragma once
#include "scene.h"
namespace Cyan 
{
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);

    struct SurfaceProperty
    {
        glm::vec3 albedo;
        glm::vec3 normal;
        glm::vec3 roughness;
        glm::vec3 metalness;
    };

    class PathTracer
    {
    public:
        PathTracer(Scene* scene);

        // inputs: scene, camera
        void progressiveRender(Scene* scene, Camera& camera);
        void run(Camera& camera);
        SurfaceProperty getSurfaceProperty(RayCastInfo& hit, glm::vec3& baryCoord);
        RayCastInfo traceScene(glm::vec3& ro, glm::vec3& rd);        
        void renderScene(Camera& camera);
        glm::vec3 renderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd);
        void bakeScene(Camera& camera);
        glm::vec3 bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd);

        void progressiveIndirectLighting();

        glm::vec3 computeDirectLighting(glm::vec3& hitPosition, glm::vec3& n);
        glm::vec3 computeDirectSkyLight(glm::vec3& ro, glm::vec3& n);
        f32 traceShadowRay(glm::vec3& ro, glm::vec3& rd);
        void setPixel(u32 px, u32 py, glm::vec3& color);

        // global illumination
        glm::vec3 recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces);

        // baking utility
        glm::vec3 sampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        void      bakeIrradianceProbe(glm::vec3& probePos, glm::ivec2& resolution);

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 2u;
        const u32 sppyCount = 2u;
        const u32 numChannelPerPixel = 3u;
        const u32 perFrameWorkGroupX = 4u;
        const u32 perFrameWorkGroupY = 4u;

        enum RenderMode
        {
            Render = 0,
            BakeLightmap
        };
        RenderMode m_renderMode;

        std::vector<RayCastInfo> m_debugRayHits;
        u32       numAccumulatedSamples;
        Camera     m_staticCamera;
        u32        m_numTracedPixels; 
        glm::uvec2 m_checkPoint;
        Texture* m_texture;
        Scene* m_scene;
        float* m_pixels;
    };
}