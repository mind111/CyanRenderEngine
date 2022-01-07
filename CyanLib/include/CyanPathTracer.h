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
        glm::vec3 irradiance;
        glm::vec3 position;
        glm::vec3 normal;
        f32       r;
    };

    struct OctreeNode
    {
        glm::vec3                      center;
        f32                            sideLength;
        OctreeNode*                    childs[8];
        std::vector<IrradianceRecord*> records;
    };

    struct Octree
    {
        Octree();
        ~Octree() { }

        static const u32        maxNodeCount = 1024 * 1024 * 200;
        u32                     m_numAllocatedNodes;
        std::vector<OctreeNode> m_nodePool;
        OctreeNode*             m_root;

        void        init(const glm::vec3& center, f32 sideLength);
        void        insert(IrradianceRecord* newRecord);
        u32         getChildIndexEnclosingSurfel(OctreeNode* node, glm::vec3& position);
        OctreeNode* allocNode();
    };

    // todo: debug visualize all the hemisphere sampled irradiance samples
    struct IrradianceCache
    {
        IrradianceCache();
        ~IrradianceCache() {}

        void init(std::vector<SceneNode*>& nodes);
        void addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r);
        void findValidRecords(std::vector<IrradianceRecord*>& validSet, const glm::vec3& p, const glm::vec3& pn);

        static const u32 cacheSize = 1024 * 1024 * 100;
        std::vector<IrradianceRecord> m_cache;
        u32 m_numRecords;
        Octree* m_octree;
    };

    struct Sphere
    {
        glm::vec3 center;
        f32       radius;
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
        
        // irradiance caching
        void      fastRenderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays);
        glm::vec3 fastRenderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        glm::vec3 fastRenderScene(Camera& camera);
        void      debugRender();

        glm::vec3 bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        void      postProcess();

        glm::vec3 computeDirectLighting(glm::vec3& hitPosition, glm::vec3& n);
        glm::vec3 computeDirectSkyLight(glm::vec3& ro, glm::vec3& n);
        f32       traceShadowRay(const glm::vec3& ro, glm::vec3& rd);
        void      setPixel(u32 px, u32 py, const glm::vec3& color);

        // global illumination
        glm::vec3 recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces, TriMaterial& matl);
        glm::vec3 irradianceCaching(glm::vec3& p, glm::vec3& pn);

        // utility
        f32       sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples);
        glm::vec3 importanceSampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        glm::vec3 sampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        glm::vec3 sampleNewIrradianceRecord(glm::vec3& p, glm::vec3& n);
        void      bakeIrradianceProbe(glm::vec3& probePos, glm::ivec2& resolution);

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 4u;
        const u32 sppyCount = 4u;
        const u32 numChannelPerPixel = 3u;
        u32       numIndirectBounce = 3u;

        enum RenderMode
        {
            Render = 0,
            BakeLightmap
        } m_renderMode;

        glm::vec3                m_skyColor;
        Texture*                 m_texture;
        Scene*                   m_scene;
        std::vector<Entity*>     m_staticEntities;
        std::vector<SceneNode*>  m_staticSceneNodes;
        IrradianceCache*         m_irradianceCache;
        std::vector<TriMaterial> m_sceneMaterials;
        float*                   m_pixels;
        static std::atomic<u32>  progressCounter;
        static PathTracer*       m_singleton;

        struct DebugObjects
        {
            std::vector<BoundingBox3f> octreeBoundingBoxes;
            std::vector<Sphere>        debugSpheres;
        } m_debugObjects;
    };
}