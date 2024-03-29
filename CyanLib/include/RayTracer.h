#pragma once

#include <functional>

#include "scene.h"
#include "RayTracingScene.h"

namespace Cyan 
{
    struct Ray
    {
        glm::vec3 ro; 
        glm::vec3 rd;
    };

    struct RayHit
    {
        f32 t;
        i32 tri;
        i32 material;
    };

    struct Image
    {
        Image(const glm::uvec2& inImageSize)
            : size(inImageSize)
        {
            u32 numPixels = inImageSize.x * inImageSize.y;
            pixels.resize(numPixels);
        }

        void setPixel(const glm::uvec2& coords, const glm::vec3& albedo) 
        { 
            u32 index = coords.y * size.x + coords.x;
            if (index < pixels.size())
            {
                pixels[index] = albedo;
            }
        }

        void clear()
        {
            for (auto& pixel : pixels)
            {
                pixel = glm::vec3(0.f);
            }
        }

        glm::uvec2 size;
        std::vector<glm::vec3> pixels;
    };

    glm::vec3 calcBarycentricCoords(const glm::vec3& p, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

    // utility functions
    template <typename T>
    T barycentricLerp(const T& interpolants0, const T& interpolants1, const T& interpolants2, const glm::vec3& barycentrics)
    {

    }

    class RayTracer
    {
    public:
        struct RenderTracker
        {
            const Image* outImage = nullptr;
            f32 progress = 0.f;

            void reset()
            {
                outImage = nullptr;
                progress = 0.f;
            }
        };

        RayTracer() { }
        bool busy() { return isBusy; }
        f32 getProgress() { return m_renderTracker.progress; }

        void renderScene(const RayTracingScene& rtxScene, const PerspectiveCamera& camera, Image& outImage, const std::function<void()>& finishCallback);
        RayHit trace(const RayTracingScene& rtxScene, const Ray& ray);
    private:

        void onRenderStart(Image& outImage) 
        {
            isBusy = true;
            m_renderTracker.outImage = &outImage;
        }

        void onRenderFinish(const std::function<void()>& callback) 
        { 
            isBusy = false;
            m_renderTracker.reset();
            callback();
        }

        RenderTracker m_renderTracker = { };
        bool isBusy = false;
    };

#if 0
    struct IrradianceRecord;

    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);

    struct Ray
    {
        glm::vec3 ro;
        glm::vec3 rd;
    };

    struct Sphere
    {
        glm::vec3 center;
        f32       radius;
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
        glm::vec3        center;
        f32              sideLength;
        OctreeNode*      childs[8];
        std::vector<u32> records;
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
        OctreeNode* allocNode();
        u32         getChildIndexEnclosingSurfel(OctreeNode* node, const glm::vec3& position);
        void        traverse(const std::function<void(std::queue<OctreeNode*>&, OctreeNode*)>& callback);
        void        insert(const IrradianceRecord& newRecord, u32 recordIndex);
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
        // a gradient vector for each albedo component R, G, B
        glm::vec3 gradient_r[3];
        glm::vec3 gradient_t[3];
    };

    /* @brief:
    * todo: shared gradients
    * todo: neighbor clamping 
    */
    struct IrradianceCache
    {
        IrradianceCache();
        ~IrradianceCache() {}

        void init(std::vector<SceneNode*>& nodes);
        const IrradianceRecord& addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r, const glm::vec3* rotationalGradient, const glm::vec3* translationalGradient);

        static const u32 cacheSize = 1024 * 1024;
        std::vector<IrradianceRecord> m_records;
        u32 m_numRecords;
        Octree* m_octree;
    };

    class PathTracer
    {
    public:

        PathTracer();

        static PathTracer* get();
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

        // shared caching
        void      fastRenderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays);
        glm::vec3 fastShadeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl);
        void      fastRenderScene(Camera& camera);
        void      debugIrradianceCache(Camera& camera);

        struct IrradianceCacheConfig
        {
            const f32 kError = 0.2f;
            const f32 kSmoothing = 1.6f;
        } m_irradianceCacheCfg;

        /*
        * Approximate diffuse interreflection using Irradiance Caching presented in [Ward88] "A Ray Tracing Solution for Diffuse Interreflection" 
        */
        glm::vec3 approximateDiffuseInterreflection(glm::vec3& p, glm::vec3& pn, f32 error);

        /*
        * Fill up an shared cache by tracing rays into scene to sample shared and cache sampled shared into shared cache. This pass doesn't
        * do any shading. Building the shared cache in a separate first pass greatly improves quality of the interpolation performed during the shading pass.
        * This greatly alleviates the interpolation artifacts caused by the fact new shared records doesn't affect any shared values that were interpolated
        * before them.
        */
        void      fillIrradianceCache(const std::vector<Ray>& rays, u32 start, u32 end, Camera& camera);

        /*
        * Sample shared at a point on surface using hemi-sphere sampling. This gets called when shared caching algorithm failed to find any
        * cached shared records that can be used to interploate to get shared at 'p'
        */
        const IrradianceRecord& sampleIrradianceRecord(glm::vec3& p, glm::vec3& n);

        // utility
        f32       sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples);
        glm::vec3 importanceSampleIrradiance(glm::vec3& samplePos, glm::vec3& n);
        glm::vec3 sampleIrradiance(glm::vec3& samplePos, glm::vec3& n);

        struct DebugData
        {
            std::vector<BoundingBox3D> octreeBoundingBoxes;
            std::vector<Ray> debugHemisphereSamples;
            std::vector<glm::vec3> translationalGradients;
            glm::vec3 pos;
            glm::vec3 tangentFrame[3];
            glm::vec3 ukvk[2];
            std::vector<u32> lerpRecords;
        } m_debugData;

        void      fillIrradianceCacheDebugData(Camera& camera);
        glm::vec3 cachedIrradiance;
        glm::vec3 interpolatedIrradiance;

        // constants
        const u32 numPixelsInX = 640u;
        const u32 numPixelsInY = 360u;
        const u32 sppxCount = 1u;
        const u32 sppyCount = 1u;
        const u32 numChannelPerPixel = 3u;
        u32       numIndirectBounce = 1u;

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
#endif
}