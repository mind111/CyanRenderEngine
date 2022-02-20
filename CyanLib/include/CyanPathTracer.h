#pragma once
#include "scene.h"
namespace Cyan 
{
    struct IrradianceRecord;

    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);

    struct Ray
    {
        glm::vec3 ro;
        glm::vec3 rd;
    };

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

        static const u32        maxNodeCount = 1024 * 1024;
        u32                     m_numAllocatedNodes;
        std::vector<OctreeNode> m_nodePool;
        OctreeNode*             m_root;

        void        init(const glm::vec3& center, f32 sideLength);
        void        insert(IrradianceRecord* newRecord);
        u32         getChildIndexEnclosingSurfel(OctreeNode* node, glm::vec3& position);
        OctreeNode* allocNode();
    };

    /* @brief:
    *  
    */
    struct IrradianceRecord
    {
        glm::vec3 irradiance;
        glm::vec3 position;
        glm::vec3 normal;
        f32       r;
        // a gradient vector for each color component R, G, B
        glm::vec3 gradient_r[3];
        glm::vec3 gradient_t[3];
    };

    /* @brief:
    * todo: irradiance gradients
    * todo: octree traversal for finding a valid set of irradiance records 
    * todo: neighbor clamping 
    * fix:  using distance to closest visible surface as Ri makes irradiance records too dense! 
    */
    struct IrradianceCache
    {
        IrradianceCache();
        ~IrradianceCache() {}

        void initialize(std::vector<SceneNode*>& nodes);
        const IrradianceRecord& addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r, const glm::vec3* rotationalGradient, const glm::vec3* translationalGradient);
        void findValidRecords(std::vector<IrradianceRecord*>& validSet, const glm::vec3& p, const glm::vec3& pn, f32 error);

        static const u32 cacheSize = 1024 * 1024;
        std::vector<IrradianceRecord> m_records;
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
        void      setPixel(u32 px, u32 py, const glm::vec3& color);
        void      setScene(Scene* scene);
        void      preprocessSceneData();
        void      run(Camera& camera);
        SurfaceProperty getSurfaceProperty(RayCastInfo& hit, glm::vec3& baryCoord);
        RayCastInfo traceScene(glm::vec3& ro, glm::vec3& rd);        

        void      renderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays);
        void      renderScene(Camera& camera);
        void      renderSceneMultiThread(Camera& camera);
        glm::vec3 shadeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        glm::vec3 bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        void      postprocess();

        glm::vec3 computeDirectLighting(glm::vec3& hitPosition, glm::vec3& n);
        glm::vec3 computeDirectSkyLight(glm::vec3& ro, glm::vec3& n);
        f32       traceShadowRay(const glm::vec3& ro, glm::vec3& rd);

        /*
        * Recursively sample indirect diffuse radiance for one light path
        */
        glm::vec3 sampleIndirectDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces, TriMaterial& matl);

        // irradiance caching
        void      fastRenderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays);
        glm::vec3 fastShadeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        void      fastRenderScene(Camera& camera);
        void      debugIrradianceCache(Camera& camera);

        struct IrradianceCacheConfig
        {
            const f32 kError = 1.f;
            const f32 kSmoothing = 1.4f;
        } m_irradianceCacheCfg;

        /*
        * Approximate diffuse interreflection using Irradiance Caching presented in [Ward88] "A Ray Tracing Solution for Diffuse Interreflection" 
        */
        glm::vec3 approximateDiffuseInterreflection(glm::vec3& p, glm::vec3& pn, f32 error);

        /*
        * Fill up an irradiance cache by tracing rays into scene to sample irradiance and cache sampled irradiance into irradiance cache. This pass doesn't
        * do any shading. Building the irradiance cache in a separate first pass greatly improves quality of the interpolation performed during the shading pass.
        * This greatly alleviates the interpolation artifacts caused by the fact new irradiance records doesn't affect any irradiance values that were interpolated
        * before them.
        */
        void      fillIrradianceCache(const std::vector<Ray>& rays, u32 start, u32 end, Camera& camera);

        /*
        * Sample irradiance at a point on surface using hemi-sphere sampling. This gets called when irradiance caching algorithm failed to find any
        * cached irradiance records that can be used to interploate to get irradiance at 'p'
        */
        const IrradianceRecord& sampleIrradianceRecord(glm::vec3& p, glm::vec3& n);

        // utility
        f32       sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples);
        glm::vec3 importanceSampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        glm::vec3 sampleIrradiance(glm::vec3& samplePos, glm::vec3& n);

        struct DebugData
        {
            std::vector<glm::vec3> debugRayHitPositions;
            IrradianceRecord debugIrradianceRecord;
            std::vector<BoundingBox3f> octreeBoundingBoxes;
            std::vector<Ray> debugHemisphereSamples;
            glm::vec3 hemisphereTangentFrame[16];
            std::vector<glm::vec3> translationalGradients;
        } m_debugData;

        void      fillIrradianceCacheDebugData(Camera& camera);
        glm::vec3 m_debugPos0;
        glm::vec3 m_debugPos1;
        glm::vec3 cachedIrradiance;
        glm::vec3 interpolatedIrradiance;

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 1u;
        const u32 sppyCount = 1u;
        const u32 numChannelPerPixel = 3u;
        u32       numIndirectBounce = 1u;

        enum RenderMode
        {
            Render = 0,
            BakeLightmap
        } m_renderMode;

        glm::vec3                m_skyColor;
        Texture*                 m_texture;
        Scene*                   m_scene;
        std::vector<SceneNode*>  m_staticMeshNodes;
        std::vector<TriMaterial> m_sceneMaterials;
        IrradianceCache*         m_irradianceCache;
        IrradianceCache*         m_irradianceCacheLevels[3];
        float*                   m_pixels;
        static std::atomic<u32>  progressCounter;
        static PathTracer*       m_singleton;
    };
}