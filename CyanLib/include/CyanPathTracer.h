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

    struct TriMaterial
    {
        Texture* diffuseTex;
        glm::vec3 flatColor;
    };

    struct IrradianceRecord
    {
        glm::vec3 indirectIrradiance;
        glm::vec3 position;
        glm::vec3 normal;
    };

    struct IrradianceCache
    {
        static std::vector<IrradianceRecord> cache;
    };

    class PathTracer
    {
    public:

        PathTracer();

        static PathTracer* getSingletonPtr();
        Texture*    getRenderOutput();
        TriMaterial& getHitMaterial(RayCastInfo& hit);
        void      setScene(Scene* scene);
        void      preprocessSceneData();
        void      run(Camera& camera);
        SurfaceProperty getSurfaceProperty(RayCastInfo& hit, glm::vec3& baryCoord);
        RayCastInfo traceScene(glm::vec3& ro, glm::vec3& rd);        

        struct RayData
        {
            glm::vec3 ro;
            glm::vec3 rd;
        };
        void      renderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays);
        void      renderScene(Camera& camera);
        void      renderSceneMultiThread(Camera& camera);
        glm::vec3 renderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        glm::vec3 bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        void      postProcess();

        glm::vec3 computeDirectLighting(glm::vec3& hitPosition, glm::vec3& n);
        glm::vec3 computeDirectSkyLight(glm::vec3& ro, glm::vec3& n);
        f32       traceShadowRay(const glm::vec3& ro, glm::vec3& rd);
        void      setPixel(u32 px, u32 py, const glm::vec3& color);

        // global illumination
        glm::vec3 recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces, TriMaterial& matl);

        // baking utility
        f32       sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples);
        glm::vec3 importanceSampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        glm::vec3 sampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        void      bakeIrradianceProbe(glm::vec3& probePos, glm::ivec2& resolution);

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 1u;
        const u32 sppyCount = 1u;
        const u32 numChannelPerPixel = 3u;

        enum RenderMode
        {
            Render = 0,
            BakeLightmap
        } m_renderMode;

        glm::vec3 m_skyColor;
        Texture* m_texture;
        Scene* m_scene;
        std::vector<Entity*> m_staticEntities;
        std::vector<TriMaterial> m_sceneMaterials;
        float* m_pixels;

        static std::atomic<u32> progressCounter;
        static PathTracer* m_singleton;
    };
}