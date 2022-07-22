#include <thread>
#include <queue>
#include <atomic>

#include "Ray.h"
#include "RayTracer.h"
#include "Texture.h"
#include "MathUtils.h"
#include "CyanAPI.h"

/* 
    * todo: (Feature) integrate intel's open image denoiser
*/

// irradiance caching pipelineState
#define DEBUG_IC
#if 1
    #define USE_IC
#endif
#define POST_PROCESSING
#if 0
    #define USE_SHORTEST_RI
#else
    #define USE_HARMONIC_MEAN_RI
#endif
#if 0
    #define VISUALIZE_IRRADIANCE
#endif

// Is 2mm of an offset too large..?
// #define EPSILON              0.002f

namespace Cyan
{
    static glm::vec3 shade(const RayTracingScene& rtxScene, const RayHit& hit);

    // taken from https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    static f32 intersect(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
    {
        const float EPSILON = 0.0000001;
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h, s, q;
        float a,f,u,v;
        h = glm::cross(ray.rd, edge2);
        // a = glm::dot(edge1, h);
        a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
        if (fabs(a) < EPSILON)
        {
            return -1.0f;
        }
        f = 1.0f / a;
        s = ray.ro - v0;
        // u = f * dot(s, h);
        u = f * (s.x*h.x + s.y*h.y + s.z*h.z);
        if (u < 0.0 || u > 1.0)
        {
            return -1.0;
        }
        q = glm::cross(s, edge1);
        v = f * (ray.rd.x*q.x + ray.rd.y*q.y + ray.rd.z*q.z);
        if (v < 0.0 || u + v > 1.0)
        {
            return -1.0;
        }
        float t = f * glm::dot(edge2, q);
        // hit
        if (t > EPSILON)
        {
            return t;
        }
        return -1.0f;
    }

    static Ray generateRay(const PerspectiveCamera& camera, const glm::vec2& pixelCoords)
    {
        glm::vec2 uv = pixelCoords * 2.f - 1.f;
        uv.x *= camera.aspectRatio;
        f32 h = camera.n * glm::tan(glm::radians(.5f * camera.fov));
        glm::vec3 ro = camera.position;
        glm::vec3 rd = normalize(
              camera.forward() * camera.n
            + camera.right() * uv.x * h
            + camera.up() * uv.y * h);
        return Ray{ ro, rd };
    }

    void RayTracer::renderScene(const RayTracingScene& rtxScene, const PerspectiveCamera& camera, Image& outImage, const std::function<void()>& finishCallback)
    {
        onRenderStart(outImage);
        {
            for (u32 y = 0; y < outImage.size.y; ++y)
            {
                for (u32 x = 0; x < outImage.size.x; ++x)
                {
                    glm::vec2 pixelCoords((f32)x / outImage.size.x, (f32)y / outImage.size.y);
                    Ray ray = generateRay(camera, pixelCoords);
                    RayHit hit = trace(rtxScene, ray);

                    if (hit.tri >= 0)
                    {
                        outImage.setPixel(glm::uvec2(x, y), shade(rtxScene, hit));
                    }

                    m_renderTracker.progress = (f32)(y * outImage.size.x + x) / (outImage.size.x * outImage.size.y);
                }
            }
        }
        onRenderFinish(finishCallback);
    }

    RayHit RayTracer::trace(const RayTracingScene& rtxScene, const Ray& ray)
    {
        f32 closestT = FLT_MAX;
        i32 hitTri = -1;
        i32 material = -1;

        // brute force tracing
        {
            for (u32 tri = 0; tri < rtxScene.surfaces.numTriangles(); ++tri)
            {
                f32 t = intersect(
                    ray,
                    rtxScene.surfaces.positions[tri * 3 + 0],
                    rtxScene.surfaces.positions[tri * 3 + 1],
                    rtxScene.surfaces.positions[tri * 3 + 2]
                );

                if (t > 0.f && t < closestT)
                {
                    closestT = t;
                    hitTri = tri;
                    material = rtxScene.surfaces.materials[tri];
                }
            }
        }
        // bvh tracing
        {

        }

        return RayHit{ closestT, hitTri, material };
    }

    glm::vec3 calcBarycentricCoords(const glm::vec3& p, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
    {
        f32 totalArea = glm::length(glm::cross(v1 - v0, v2 - v0));

        f32 w = glm::length(glm::cross(v1 - v0, p - v0)) / totalArea;
        f32 v = glm::length(glm::cross(v2 - v0, p - v0)) / totalArea;
        f32 u = 1.f - v - w;
        return glm::vec3(u, v, w);
    }

    static glm::vec3 shade(const RayTracingScene& rtxScene, const RayHit& hit)
    {
        glm::vec3 outRadiance(0.f);
        const auto& material = rtxScene.materials[hit.material];
#if 0 
        glm::vec3 barycentrics = calcBarycentricCoords();
        glm::vec3 n = barycentricLerp(
            barycentrics, 
            rtxScene.surfaces.positions[hit.tri * 3 + 0],
            rtxScene.surfaces.positions[hit.tri * 3 + 1],
            rtxScene.surfaces.positions[hit.tri * 3 + 2]
        );
#else
        glm::vec3 n = glm::normalize(
            rtxScene.surfaces.normals[hit.tri * 3 + 0]
          + rtxScene.surfaces.normals[hit.tri * 3 + 1]
          + rtxScene.surfaces.normals[hit.tri * 3 + 2]
        );
        f32 ndotl = max(glm::dot(n, glm::vec3(1.f, 0.f, 0.f)), 0.f);
        outRadiance += glm::vec3(0.2) * material.albedo;
        outRadiance += ndotl * glm::vec3(1.f) * material.albedo;
#endif
        return outRadiance;
    }

#if 0
    f32 saturate(f32 val);
    Ray generateRay(const glm::uvec2& pixelCoord, Camera& camera, const glm::uvec2& screenDim);
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord);
    glm::vec3 ACESFilm(const glm::vec3& color);
    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma);
    inline f32 getLuminance(const glm::vec3& color);
    inline f32 getSpectralAverage(const glm::vec3& rgb);
    f32 getIrradianceLerpWeight(const glm::vec3& p, const glm::vec3& pn, const IrradianceRecord& record, f32 error);

    f32 saturate(f32 val)
    {
        if (val < 0.f) val = 0.f;
        if (val > 1.f) val = 1.f;
        return val;
    }

    // reference: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    glm::vec3 ACESFilm(const glm::vec3& x)
    {
        f32 a = 2.51f;
        f32 b = 0.03f;
        f32 c = 2.43f;
        f32 d = 0.59f;
        f32 e = 0.14f;
        glm::vec3 result = (x*(a*x+b))/(x*(c*x+d)+e);
        return glm::vec3(saturate(result.r), saturate(result.g), saturate(result.b)); 
    }

    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma)
    {
        return glm::vec3(pow(color.r, gamma), pow(color.g, gamma), pow(color.b, gamma));
    }

    f32 getLuminance(const glm::vec3& rgb)
    {
        return 0.2126f * rgb.r + 0.7152f * rgb.g + 0.0722f * rgb.b;
    }

    f32 getSpectralAverage(const glm::vec3& rgb)
    {
        return (rgb.r + rgb.g + rgb.b) / 3.f;
    }

    Ray generateRay(const glm::uvec2& pixelCoord, Camera& camera, const glm::uvec2& screenDim)
    {
        glm::vec2 uv;
        uv.x = (f32)pixelCoord.x / screenDim.x * 2.f - 1.f;
        uv.y = (f32)pixelCoord.y / screenDim.y * 2.f - 1.f;
        uv.x *= (f32)screenDim.x / screenDim.y;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        glm::vec3 ro = camera.position;
        glm::vec3 rd = normalize(
              camera.forward * camera.n
            + camera.right   * uv.x * w
            + camera.up      * uv.y * w);
        return {ro, rd};
    }

#if 0
    // hit need to be in object space
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace)
    {
        auto p = hitPosObjectSpace;
        auto p0 = tri.m_vertices[0];
        auto p1 = tri.m_vertices[1];
        auto p2 = tri.m_vertices[2];

        f32 totalArea = glm::length(glm::cross(p1 - p0, p2 - p0));

        f32 w = glm::length(glm::cross(p1 - p0, p - p0)) / totalArea;
        f32 v = glm::length(glm::cross(p2 - p0, p - p0)) / totalArea;
        f32 u = 1.f - v - w;
        return glm::vec3(u, v, w);
    }

    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition)
    {
        auto worldTransformMatrix = rayHit.m_node->getWorldTransform().toMatrix();
        // convert world space hit back to object space
        glm::vec3 hitPosObjectSpace = vec4ToVec3(glm::inverse(worldTransformMatrix) * glm::vec4(hitPosition, 1.f));

        auto mesh = rayHit.m_node->m_meshInstance->m_mesh;
        auto tri = mesh->getTriangle(rayHit.smIndex, rayHit.triIndex);
        return computeBaryCoord(tri, hitPosObjectSpace);
    }
#endif

    // TODO: bilinear texture filtering
    glm::vec3 texelFetch(Texture* texture, glm::vec3& texCoord)
    {
        // how to interpret the data should be according to texture format
        u32 px = (u32)roundf(texCoord.x * texture->width);
        u32 py = (u32)roundf(texCoord.y * texture->height);
        // assuming rgb one byte per pixel
        u32 bytesPerPixel = 4;
        u32 flattendPixelIndex = (py * texture->width + px);
        u32 bytesOffset = flattendPixelIndex * bytesPerPixel;
        u8* pixelAddress = ((u8*)texture->data + bytesOffset);
        u8 r = *pixelAddress;
        u8 g = *(pixelAddress + 1);
        u8 b = *(pixelAddress + 2);
        return glm::vec3((r / 255.f), (g / 255.f), (b / 255.f));
    }

    glm::vec3 texelFetchCube(Texture* cubemap, glm::vec3 rd)
    {
        return glm::vec3(0.f);
    }

    MaterialInstance* getSurfaceMaterial(RayCastInfo& rayHit)
    {
        return nullptr;
    }

#if 0
    // return surface normal at ray hit in world space
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord)
    {
        auto mesh = rayHit.m_node->m_meshInstance->m_mesh;
        auto sm = mesh->m_subMeshes[rayHit.smIndex];
        u32 vertexOffset = rayHit.triIndex * 3;
        glm::vec3 normal = baryCoord.x * sm->m_triangles.normals[vertexOffset]
            + baryCoord.y * sm->m_triangles.normals[vertexOffset + 1]
            + baryCoord.z * sm->m_triangles.normals[vertexOffset + 2];
        normal = glm::normalize(normal);
        // convert normal back to world space
        auto worldTransformMatrix = rayHit.m_node->getWorldTransform().toMatrix();
        normal = vec4ToVec3(glm::inverse(glm::transpose(worldTransformMatrix)) * glm::vec4(normal, 0.f));
        return glm::normalize(normal);
    }
#endif
    PathTracer* PathTracer::m_singleton = nullptr;
    std::atomic<u32> PathTracer::progressCounter(0u);

    PathTracer::PathTracer()
        : m_scene(nullptr)
        , m_pixels(nullptr)
        , m_texture(nullptr)
        , m_skyColor(0.328f, 0.467f, 1.f)
        , m_irradianceCache(nullptr)
        , m_irradianceCacheLevels{ nullptr, nullptr, nullptr }
    {
        if (!m_singleton)
        {
            u32 bytesPerPixel = sizeof(float) * 3;
            u32 numPixels = numPixelsInX * numPixelsInY;
            m_pixels = (float*)new char[(u64)(bytesPerPixel * numPixels)];
            auto textureManager = TextureManager::get();
            TextureSpec spec = { };
            spec.type = Texture::Type::TEX_2D;
            spec.width = numPixelsInX;
            spec.height = numPixelsInY;
            spec.format = Texture::ColorFormat::R32G32B32;
            spec.type = Texture::Type::TEX_2D;
            spec.dataType = Texture::DataType::Float;
            m_texture = textureManager->createTexture("PathTracingOutput", spec);
            m_singleton = this;
            m_irradianceCache = new IrradianceCache;
            for (u32 i = 0; i < 3; ++i)
                m_irradianceCacheLevels[i] = new IrradianceCache;
        }
        else
            cyanError("Multiple instances are created for PathTracer!");
    }

    PathTracer* PathTracer::get()
    {
        return m_singleton;
    }

    Texture* PathTracer::getRenderOutput()
    {
        return m_texture;
    }

    void PathTracer::setScene(Scene* scene)
    {
        m_scene = scene;
        m_sceneMaterials.clear();
        // convert scene data to decouple ray tracing required data from raster pipeline
        preprocessSceneData();
        m_irradianceCache->init(m_staticMeshNodes);
    }

    void PathTracer::setPixel(u32 px, u32 py, const glm::vec3& color)
    {
        u32 index = (py * numPixelsInX + px) * numChannelPerPixel;
        CYAN_ASSERT(index < numPixelsInX* numPixelsInY* numChannelPerPixel, "Writing to a pixel out of bound");
        m_pixels[index + 0] = color.r;
        m_pixels[index + 1] = color.g;
        m_pixels[index + 2] = color.b;
    }

    void PathTracer::preprocessSceneData()
    {
        // todo: deduplicate material instances
        u32 matlInstanceCount = 0;
        for (u32 i = 0; i < (u32)m_scene->entities.size(); ++i)
        {
            if (m_scene->entities[i]->m_static)
            {
                std::queue<SceneNode*> nodes;
                nodes.push(m_scene->entities[i]->m_sceneRoot);
                while (!nodes.empty())
                {
                    auto* node = nodes.front();
                    nodes.pop();
                    if (node->m_meshInstance)
                    {
                        m_staticMeshNodes.push_back(node);
                        for (u32 sm = 0; sm < node->m_meshInstance->m_mesh->numSubMeshes(); ++sm)
                        {
                            node->m_meshInstance->m_rtMatls.emplace_back(matlInstanceCount++);
                            m_sceneMaterials.emplace_back();
                            auto& matl = m_sceneMaterials.back();
                            u32 materialFlags = node->m_meshInstance->m_matls[sm]->getU32("uMatlData.flags");
                            if ((materialFlags & StandardPbrMaterial::Flags::kHasDiffuseMap) == 0u)
                            {
                                // assume that the input color value is in sRGB space
                                matl.flatColor = gammaCorrect(vec4ToVec3(node->m_meshInstance->m_matls[sm]->getVec4("uMatlData.flatColor")), 2.2f);
                                matl.diffuseTex = nullptr;
                            }
                        }
                    }
                    for (u32 i = 0; i < node->m_child.size(); ++i)
                        nodes.push(node->m_child[i]);
                }
            }
        }
    }

    void PathTracer::run(Camera& camera)
    {
        CYAN_ASSERT(m_scene, "PathTracer::m_scene is NULL when PathTracer::run() is called!")

#ifdef USE_IC
        // render the scene using irradiance caching
        fastRenderScene(camera);
#else
        // render the scene using brute-force path tracing
        renderSceneMultiThread(camera);
#endif
    }

    glm::vec3 PathTracer::bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl)
    {
        glm::vec3 radiance(0.f);
        if (hit.t > 0.f)
        {
            glm::vec3 hitPosition = ro + rd * hit.t;
            auto mesh = hit.m_node->m_meshInstance->m_mesh;
            auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex); 
            auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
#if 0
            {
                // sanitize baryCoord
                if ((baryCoord.y + baryCoord.z) > 1.f)
                    m_debugRayHits.push_back(hit);
            }
#endif
            glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
            // back face hit
            if (dot(normal, rd) > 0) return radiance;

            // bake direct static sky light & indirect lighting
            glm::vec3 indirectRo = hitPosition + EPSILON * normal;
            radiance += computeDirectSkyLight(indirectRo, normal);
            radiance += sampleIndirectDiffuse(indirectRo, normal, 0, getHitMaterial(hit));
        }
        return radiance;
    }

    SurfaceProperty PathTracer::getSurfaceProperty(RayCastInfo& hit, glm::vec3& baryCoord)
    {
        SurfaceProperty props = { };

        auto mesh = hit.m_node->m_meshInstance->m_mesh;
        auto sm = mesh->m_subMeshes[hit.smIndex];
        u32 vertexOffset = hit.triIndex * 3;
        // normal
        {
            props.normal = baryCoord.x * sm->m_triangles.normals[vertexOffset]
                        + baryCoord.y * sm->m_triangles.normals[vertexOffset + 1]
                        + baryCoord.z * sm->m_triangles.normals[vertexOffset + 2];
            props.normal = glm::normalize(props.normal);
            // convert normal back to world space
            auto worldTransformMatrix = hit.m_node->getWorldTransform().toMatrix();
            props.normal = vec4ToVec3(glm::inverse(glm::transpose(worldTransformMatrix)) * glm::vec4(props.normal, 0.f));
            props.normal = glm::normalize(props.normal);
        }
        // albedo
        {
            props.albedo = glm::vec3(0.f);
        }
        // roughness
        {
            props.roughness = glm::vec3(0.f);
        }
        // metallic
        {
            props.metalness = glm::vec3(0.f);
        }
        return props;
    }

    RayCastInfo PathTracer::traceScene(glm::vec3& ro, glm::vec3& rd)
    {
        RayCastInfo closestHit;

        for (u32 i = 0; i < m_staticMeshNodes.size(); ++i)
        {
            glm::mat4 model = m_staticMeshNodes[i]->getWorldTransformMatrix();
            BoundingBox3D aabb = m_staticMeshNodes[i]->m_meshInstance->getAABB();

            // transform the ray into object space
            glm::vec3 roObjectSpace = ro;
            glm::vec3 rdObjectSpace = rd;
            transformRayToObjectSpace(roObjectSpace, rdObjectSpace, model);
            rdObjectSpace = glm::normalize(rdObjectSpace);

            if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = m_staticMeshNodes[i]->m_meshInstance->m_mesh;
                Cyan::MeshRayHit currentRayHit = mesh->intersectRay(roObjectSpace, rdObjectSpace);

                if (currentRayHit.t > 0.f)
                {
                    // convert hit from object space back to world space
                    auto objectSpaceHit = roObjectSpace + currentRayHit.t * rdObjectSpace;
                    f32 currentWorldSpaceDistance = transformHitFromObjectToWorldSpace(objectSpaceHit, model, ro, rd);
                    if (currentWorldSpaceDistance < closestHit.t)
                    {
                        closestHit.t = currentWorldSpaceDistance;
                        closestHit.smIndex = currentRayHit.smIndex;
                        closestHit.triIndex = currentRayHit.triangleIndex;
                        closestHit.m_node = m_staticMeshNodes[i];
                    }
                }
            }
        }

        if (!closestHit.m_node)
            closestHit.t = -1.f;
        return closestHit;
    }

    TriMaterial& PathTracer::getHitMaterial(RayCastInfo& hit)
    {
        return m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
    }

    glm::vec3 PathTracer::shadeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl)
    {
        glm::vec3 radiance(0.f);
        glm::vec3 hitPosition = ro + rd * hit.t;
        auto mesh = hit.m_node->m_meshInstance->m_mesh;
        auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex); 
        auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;

        // compute barycentric coord of the surface point that we hit 
        glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
        glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
        hitPosition += normal * EPSILON;

        // back face hit
        if (dot(normal, rd) > 0) return radiance;

        // direct + indirect (diffuse interreflection)
        radiance += sampleIndirectDiffuse(hitPosition, normal, numIndirectBounce, getHitMaterial(hit));
        //radiance *= sampleAo(hitPosition, normal, 8);
        return radiance;
    }

    void PathTracer::postprocess()
    {
        // not implemented
    }


    f32 PathTracer::sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples)
    {
        f32 visibility = 1.f;
        for (u32 i = 0; i < numSamples; ++i)
        {
            glm::vec3 sampleDir = cosineWeightedSampleHemisphere(n);
            auto hit = m_scene->castRay(samplePos, sampleDir, EntityFilter::BakeInLightMap);
            if (hit.t > 0.f)
            {
                visibility -= (1.f / (f32)numSamples) * saturate(1.f / (1.f + hit.t));
            }
        }
        return visibility;
    }

    glm::vec3 PathTracer::sampleIrradiance(glm::vec3& samplePos, glm::vec3& n)
    {
        const u32 numSamples = 32;
        glm::vec3 irradiance(0.f);
        for (u32 i = 0; i < numSamples; ++i)
        {
            auto rd = uniformSampleHemisphere(n);
            auto hit = traceScene(samplePos, rd);
            if (hit.t > 0.f)
            {
                auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                // todo: instead of only consider diffuse reflections along the path, also include indirect specular
                auto radiance = bakeSurface(hit, samplePos, rd, matl);
                irradiance += radiance * max(dot(rd, n), 0.f) * (1.f / M_PI);
            }
            else
                irradiance += m_skyColor * max(dot(rd, n), 0.f);
        }
        irradiance /= numSamples;
        return irradiance;
    }

    glm::vec3 PathTracer::importanceSampleIrradiance(glm::vec3& samplePos, glm::vec3& n)
    {
        const u32 numSamples = 64;
        glm::vec3 irradiance(0.f);
        for (u32 i = 0; i < numSamples; ++i)
        {
            auto rd = cosineWeightedSampleHemisphere(n);
            auto hit = traceScene(samplePos, rd);
            if (hit.t > 0.f)
            {
                auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                auto radiance = shadeSurface(hit, samplePos, rd, matl);
                irradiance += radiance;
            }
            else
                irradiance += m_skyColor;
        }
        irradiance /= numSamples;
        return irradiance;
    }

    void PathTracer::renderScene(Camera& camera)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        u32 totalNumRays = numPixelsInX * numPixelsInY;
        f32 pixelSize = 1.f / (f32)numPixelsInY;
        f32 subPixelSize = pixelSize / (f32)sppxCount;
        u32 subPixelSampleCount = sppxCount * sppyCount;
        totalNumRays *= subPixelSampleCount;
        u32 numTracedRays = 0;

        for (u32 x = 0u; x < numPixelsInX; ++x)
        {
            for (u32 y = 0u; y < numPixelsInY; ++y)
            {
                glm::vec3 pixelColor(0.f);
                glm::vec2 pixelCenterCoord;
                pixelCenterCoord.x = ((f32)x / (f32)numPixelsInX) * 2.f - 1.f;
                pixelCenterCoord.y = ((f32)y / (f32)numPixelsInY) * 2.f - 1.f;

                // stratified sampling
                for (u32 spx = 0; spx < sppxCount; ++spx)
                {
                    for (u32 spy = 0; spy < sppyCount; ++spy)
                    {
                        glm::vec2 subPixelOffset((f32)spx * subPixelSize, (f32)spy * subPixelSize);
                        // jitter the sub pixel samples
                        glm::vec2 jitter = glm::vec2(subPixelSize) * glm::vec2(uniformSampleZeroToOne(), uniformSampleZeroToOne());
                        subPixelOffset += jitter;
                        glm::vec2 sampleScreenCoord = pixelCenterCoord + subPixelOffset;
                        sampleScreenCoord.x *= aspectRatio;

                        glm::vec3 ro = camera.position;
                        glm::vec3 rd = normalize(camera.forward * camera.n
                            + camera.right * sampleScreenCoord.x * w
                            + camera.up * sampleScreenCoord.y * w);

                        auto hit = traceScene(ro, rd);
                        if (hit.t > 0.f)
                            pixelColor += shadeSurface(hit, ro, rd, getHitMaterial(hit));

                        {
                            numTracedRays += 1;
                            printf("\r[Info] Traced %u/%u  ...%.2f%", numTracedRays, totalNumRays, (f32)numTracedRays * 100.f / totalNumRays);
                            fflush(stdout);
                        }
                    }
                }
                setPixel(x, y, pixelColor / (f32)subPixelSampleCount);
            }
        }

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void PathTracer::renderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        f32 pixelSize = 1.f / (f32)numPixelsInY;
        f32 subPixelSize = pixelSize / (f32)sppxCount;

        for (u32 i = start; i < end; ++i)
        {
            u32 py = i / numPixelsInX;
            u32 px = i % numPixelsInX;
            glm::vec2 pixelCenterCoord;
            pixelCenterCoord.x = ((f32)px / (f32)numPixelsInX) * 2.f - 1.f;
            pixelCenterCoord.y = ((f32)py / (f32)numPixelsInY) * 2.f - 1.f;

            // stratified sampling
            glm::vec3 color(0.f);
            for (u32 spx = 0; spx < sppxCount; ++spx)
            {
                for (u32 spy = 0; spy < sppyCount; ++spy)
                {
                    glm::vec2 subPixelOffset((f32)spx * subPixelSize, (f32)spy * subPixelSize);
                    // jitter the sub pixel samples
                    glm::vec2 jitter = glm::vec2(subPixelSize) * glm::vec2(uniformSampleZeroToOne(), uniformSampleZeroToOne());
                    subPixelOffset += jitter;
                    glm::vec2 sampleScreenCoord = pixelCenterCoord + subPixelOffset;
                    sampleScreenCoord.x *= aspectRatio;

                    glm::vec3 ro = camera.position;
                    glm::vec3 rd = normalize(camera.forward * camera.n
                        + camera.right * sampleScreenCoord.x * w
                        + camera.up * sampleScreenCoord.y * w);
                    auto hit = traceScene(ro, rd);
                    if (hit.t > 0.f)
                    {
#ifdef VISUALIZE_IRRADIANCE
                        glm::vec3 hitPosition = ro + rd * hit.t;
                        auto mesh = hit.m_node->m_meshInstance->m_mesh;
                        auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex);
                        auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;
                        // compute barycentric coord of the surface point that we hit 
                        glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
                        glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
                        hitPosition += normal * EPSILON;
                        color += sampleIrradiance(hitPosition, normal) * getHitMaterial(hit).flatColor;
#else
                        color += shadeSurface(hit, ro, rd, getHitMaterial(hit));
#endif
                    }
                    progressCounter.fetch_add(1);
                }
            }
            color /= (sppxCount * sppyCount);
#ifdef POST_PROCESSING
            // post-processing
            {
                // tone mapping
                auto toneMappedColor = ACESFilm(color);
                // gamma correction
                auto finalColor = gammaCorrect(toneMappedColor, .4545f);
                setPixel(px, py, finalColor);
            }
#else
            setPixel(px, py, color);
#endif
        }
    }

    void PathTracer::renderSceneMultiThread(Camera& camera)
    {
        Cyan::Toolkit::ScopedTimer timer("PathTracer::renderSceneMultiThread", true);
        u32 numPixels = numPixelsInX * numPixelsInY;
        u32 totalNumRays = numPixels * sppxCount * sppyCount;

        // trace all the rays using multi-threads
        const u32 workGroupCount = 16;
        u32 workGroupPixelCount = numPixels / workGroupCount;
        // divide work into 16 threads
        std::thread* workers[workGroupCount] = { };
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = workGroupPixelCount * i;
            u32 end = min(start + workGroupPixelCount, numPixels);
            workers[i] = new std::thread(&PathTracer::renderWorker, this, start, end, std::ref(camera), totalNumRays);
        }

        // let the main thread monitor the progress
        while (progressCounter.load() < totalNumRays)
        {
            printf("\r[Info] Traced %d rays ... %.2f%%", progressCounter.load(), ((f32)progressCounter.load() * 100.f / totalNumRays));
            fflush(stdout);
        }

        for (u32 i = 0; i < workGroupCount; ++i)
            workers[i]->join();
        printf("\n");

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // free resources
        progressCounter = 0;
        for (u32 i = 0; i < workGroupCount; ++i)
            delete workers[i];
    }

    f32 PathTracer::traceShadowRay(const glm::vec3& ro, glm::vec3& rd)
    {
        f32 shadow = 1.f;
        if (m_scene->castVisibilityRay(ro, rd, EntityFilter::BakeInLightMap))
            shadow = 0.f;
        return shadow;
    }

    glm::vec3 PathTracer::computeDirectLighting(glm::vec3& hitPosition, glm::vec3& normal)
    {
        glm::vec3 color(0.f);
        for (auto dirLight : m_scene->dLights)
        {
            glm::vec3 lightDir = dirLight.direction;
            // trace shadow ray
            f32 shadow = traceShadowRay(hitPosition, lightDir);
            f32 ndotl = max(dot(normal, lightDir), 0.f);
            glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
            color += Li * ndotl;
        }
        return color;
    }

    glm::vec3 PathTracer::computeDirectSkyLight(glm::vec3& ro, glm::vec3& n)
    {
        // TODO: sample skybox instead of using a constant sky color
        glm::vec3 outColor(0.f);
        const u32 sampleCount = 1u;
        for (u32 i = 0u; i < sampleCount; ++i)
        {
            glm::vec3 sampleDir = uniformSampleHemisphere(n);
            // the ray hit the skybox
            if (!m_scene->castVisibilityRay(ro, sampleDir, EntityFilter::BakeInLightMap))
                outColor += m_skyColor * max(dot(sampleDir, n), 0.f);
        }
        return outColor / (f32)sampleCount;
    }

    glm::vec3 PathTracer::sampleIndirectDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces, TriMaterial& matl)
    {
        glm::vec3 exitRadiance(0.f);

        // direct 
        exitRadiance += computeDirectSkyLight(ro, n);
        exitRadiance += computeDirectLighting(ro, n);
        // only consider the pi term for direct lighting, including the pi term in lambertian brdf in the indirect lighting darkens
        // the indirect lighting too much
        exitRadiance *= INV_PI;

        // stop recursion
        if (numBounces == 0)
            return exitRadiance * (matl.flatColor);

        glm::vec3 rd = cosineWeightedSampleHemisphere(n);

        // ray cast
        auto nextRayHit = m_scene->castRay(ro, rd, EntityFilter::BakeInLightMap);

        if (nextRayHit.t > 0.f)
        {
            // compute geometric information
            glm::vec3 nextBounceRo = ro + nextRayHit.t * rd;
            glm::vec3 baryCoord = computeBaryCoordFromHit(nextRayHit, nextBounceRo);
            glm::vec3 nextBounceNormal = getSurfaceNormal(nextRayHit, baryCoord);
            auto      nextMatl = getHitMaterial(nextRayHit);

            // avoid self-intersecting
            nextBounceRo += EPSILON * nextBounceNormal;

            // indirect
            f32 indirectBoost = 1.0f;

            // the cosine geometric term is cancelled with the pdf
            exitRadiance += (indirectBoost * sampleIndirectDiffuse(nextBounceRo, nextBounceNormal, numBounces - 1, nextMatl));
        }
        return exitRadiance * matl.flatColor;
    }

    struct HemisphereCellData
    {
        glm::vec3 radiance;
        f32 r;
    };

    struct IrradianceGradients
    {
        glm::vec3 t[3];
        glm::vec3 r[3];
    };

    void computeIrradianceGradients(IrradianceGradients& gradients, HemisphereCellData** cells, f32 M, f32 N, const glm::vec3& n)
    {
        auto tangentFrame = tangentToWorld(n);
        auto tangent = tangentFrame[0];
        auto bitangent = tangentFrame[1];
        f32 dPhi = 2 * M_PI / N;

        f32 rMin = FLT_MAX;
        for (u32 k = 0; k < N; ++k)
        {
            u32 sliceIndexPrev = (k - 1) % (u32)N;
            // rotate tangent vector by (k + .5f) * dPhi to get a unit vector point towards center of cell j,k 
            auto quat0 = glm::angleAxis(dPhi * ((f32)k + .5f), n);
            auto quat1 = glm::angleAxis(dPhi * k, n);
            glm::vec3 uk = glm::normalize(glm::rotate(quat0, tangent));
            glm::vec3 vk = glm::normalize(glm::rotate(quat0, bitangent));
            glm::vec3 vkPrev = glm::normalize(glm::rotate(quat1, bitangent));
            f32 magnitude_t0[3] = { 0.f, 0.f, 0.f };
            f32 magnitude_t1[3] = { 0.f, 0.f, 0.f };

            for (u32 j = 0; j < M; ++j)
            {
                f32 sinThetaMinus = sqrt((f32)j / M);
                f32 sinThetaPlus = sqrt(((f32)j + 1.f) / M);

                // rotational gradient

                // translational gradient
                if (j > 0)
                {
                    f32 denom = min(cells[k][j - 1].r, cells[k][j].r);
                    magnitude_t0[0] += sinThetaMinus * (1.f - sinThetaMinus * sinThetaMinus) * (cells[k][j].radiance.r - cells[k][j-1].radiance.r) / denom;
                    magnitude_t0[1] += sinThetaMinus * (1.f - sinThetaMinus * sinThetaMinus) * (cells[k][j].radiance.g - cells[k][j-1].radiance.g) / denom;
                    magnitude_t0[2] += sinThetaMinus * (1.f - sinThetaMinus * sinThetaMinus) * (cells[k][j].radiance.b - cells[k][j-1].radiance.b) / denom;
                }

                f32 denom = min(cells[sliceIndexPrev][j].r, cells[k][j].r);
                magnitude_t1[0] += (sinThetaPlus - sinThetaMinus) * (cells[k][j].radiance.r - cells[sliceIndexPrev][j].radiance.r) / denom;
                magnitude_t1[1] += (sinThetaPlus - sinThetaMinus) * (cells[k][j].radiance.g - cells[sliceIndexPrev][j].radiance.g) / denom;
                magnitude_t1[2] += (sinThetaPlus - sinThetaMinus) * (cells[k][j].radiance.b - cells[sliceIndexPrev][j].radiance.b) / denom;
            }
            gradients.t[0] += uk * magnitude_t0[0] * (2.f * M_PI / N) + vkPrev * magnitude_t1[0];
            gradients.t[1] += uk * magnitude_t0[1] * (2.f * M_PI / N) + vkPrev * magnitude_t1[1];
            gradients.t[2] += uk * magnitude_t0[2] * (2.f * M_PI / N) + vkPrev * magnitude_t1[2];
        }
    }

    /* Notes:
    * translational gradients still seems somewhat wrong, not sure which part is incorrect.
    * todo: cap Ri by gradient
    * todo: neighbor clamping
    */
    const IrradianceRecord& PathTracer::sampleIrradianceRecord(glm::vec3& p, glm::vec3& n)
    {
        glm::vec3 irradiance(0.f);

        // stratified sampling constants
        const f32 M = 16.f;
        const f32 N = 16.f;
        f32 numSamples = M * N;
        // N slices, each slice has M cells;
        HemisphereCellData** cells = static_cast<HemisphereCellData**>(alloca(sizeof(HemisphereCellData*) * (u64)N));
        for (u32 i = 0; i < N; ++i)
        {
            cells[i] = static_cast<HemisphereCellData*>(alloca(sizeof(HemisphereCellData) * (u64)M));
        }

        f32 Ri = 0.f, nom = 0.f;
        for (u32 k = 0; k < N; ++k)
        {
            for (u32 j = 0; j < M; ++j)
            {
                auto rd = stratifiedCosineWeightedSampleHemiSphere(n, (f32)j, (f32)k, M, N);
                auto hit = traceScene(p, rd);
                glm::vec3 radiance(0.f);
                f32 hitDistance = FLT_MAX;

                if (hit.t > 0.f)
                {
                    auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                    radiance = shadeSurface(hit, p, rd, matl);
                    irradiance += radiance;
                    hitDistance = hit.t;
                    nom += 1.f;
                }

                Ri += 1.f / hitDistance;

                cells[k][j].radiance = radiance;
                cells[k][j].r = hitDistance;
            }
        };
        Ri = nom / Ri;
        irradiance /= numSamples;

        // compute irradiance gradients
        IrradianceGradients gradients = { 
            { glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f)},
            { glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f)}
        };
        computeIrradianceGradients(gradients, cells, M, N, n);

#if 0
        // "normalize" gradients to prevent artifact
        f32 maxLength = -FLT_MAX;
        for (u32 i = 0; i < 3; ++i)
        {
            f32 magnitude = glm::length(gradients.t[i]);
            if (magnitude > maxLength)
            {
                maxLength = magnitude;
            }
        }
        f32 norm = getLuminance(irradiance) / maxLength;
        for (u32 i = 0; i < 3; ++i)
        {
            gradients.t[i] *= norm;
        }
#endif
        f32 maxLength = -FLT_MAX;
        for (u32 i = 0; i < 3; ++i)
        {
            f32 magnitude = glm::length(gradients.t[i]);
            if (magnitude > maxLength)
            {
                maxLength = magnitude;
            }
        }

        f32 R = min(Ri, 1.f / maxLength);
        // clamp sample spacing to avoid too sparse or too dense sample distribution
        Ri = glm::clamp(R, 0.1f, 2.0f);

        // debugging (translational gradients should be on the tangent plane of the hemisphere sample)
        m_debugData.translationalGradients.push_back(gradients.t[0]);

        // add to the cache
        return m_irradianceCache->addIrradianceRecord(p, n, irradiance, Ri, gradients.r, gradients.t);
    }

    glm::vec3 PathTracer::fastShadeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl)
    {
        glm::vec3 radiance(0.f);
        if (hit.t > 0.f)
        {
            glm::vec3 hitPosition = ro + rd * hit.t;
            auto mesh = hit.m_node->m_meshInstance->m_mesh;
            auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex);
            auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
            glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
            hitPosition += normal * EPSILON;

            // back face hit
            if (dot(normal, rd) > 0) return radiance;

            // direct + indirect (diffuse interreflection)
#if 0
            radiance += computeDirectLighting(hitPosition, normal);
            radiance += computeDirectSkyLight(hitPosition, normal);
#endif
            // slightly increase the error threshold for the shading pass using Irradiance Caching
            radiance += approximateDiffuseInterreflection(hitPosition, normal, m_irradianceCacheCfg.kError * m_irradianceCacheCfg.kSmoothing);
        }
        return radiance * matl.flatColor;
    }

    void PathTracer::fillIrradianceCacheDebugData(Camera& camera)
    {
#if 0
        // generate debug bounding boxes for the octree
        for (u32 i = 0; i < m_irradianceCache->m_octree->m_numAllocatedNodes; ++i)
        {
            m_debugData.octreeBoundingBoxes.emplace_back();
            auto& aabb = m_debugData.octreeBoundingBoxes.back();
            auto& octreeNode = m_irradianceCache->m_octree->m_nodePool[i];
            aabb.m_pMin = glm::vec4(octreeNode.center + glm::vec3(-.5f, -.5f, -.5f) * octreeNode.sideLength, 1.f);
            aabb.m_pMax = glm::vec4(octreeNode.center + glm::vec3( .5f,  .5f,  .5f) * octreeNode.sideLength, 1.f);
            aabb.init();
        }
#endif
        u32 px = 320, py = 240;
        auto ray = generateRay(glm::uvec2(px, py), camera, glm::uvec2(numPixelsInX, numPixelsInY));
        auto debugHit = traceScene(ray.ro, ray.rd);
        m_debugData.pos = ray.ro + debugHit.t * ray.rd;
        auto bary = computeBaryCoordFromHit(debugHit, m_debugData.pos);
        auto n = getSurfaceNormal(debugHit, bary);
        auto tangentFrame = tangentToWorld(n);
        m_debugData.tangentFrame[0] = tangentFrame[0];
        m_debugData.tangentFrame[1] = tangentFrame[1];
        m_debugData.tangentFrame[2] = tangentFrame[2];

        // capture all the records used for interpolating irradiance at current position
        m_irradianceCache->m_octree->traverse([this, &n](std::queue<OctreeNode*>& nodes, OctreeNode* node) {
            // check all the records in current node
            for (u32 i = 0; i < node->records.size(); ++i)
            {
                auto& record = m_irradianceCache->m_records[node->records[i]];
                f32 wi = getIrradianceLerpWeight(m_debugData.pos, n, record, m_irradianceCacheCfg.kError);
                // exclude cached samples that are "in front of" p
                f32 dp = dot(m_debugData.pos - record.position, (record.normal + n) * .5f);
                if (wi > 0.f && dp >= 0.f)
                {
                    m_debugData.lerpRecords.push_back(node->records[i]);
                }
            }
            // search among childs
            for (u32 i = 0; i < 8; ++i)
            { 
                auto child = node->childs[i];
                if (child)
                {
                    // recurse into this child if it has records that potentially overlaps 'p'
                    if (abs(m_debugData.pos.x - child->center.x) < child->sideLength 
                        && abs(m_debugData.pos.y - child->center.y) < child->sideLength 
                        && abs(m_debugData.pos.z - child->center.z) < child->sideLength)
                    {
                        nodes.push(child);
                    }
                }
            }
        });
    }

    void PathTracer::debugIrradianceCache(Camera& camera)
    {
        u32 numPixels = numPixelsInX * numPixelsInY;
        u32 totalNumRays = numPixels * sppxCount * sppyCount;
        const u32 workGroupCount = 1;
        u32 workGroupPixelCount = numPixels / workGroupCount;

        // divide work equally among threads
        std::thread* workers[workGroupCount] = { };
        // first pass fill-in irradiance records
        std::vector<Ray> firstPassRays;
        glm::uvec2 dim(numPixelsInX / 32, numPixelsInY / 32);
        for (u32 y = 0; y < dim.y; ++y)
        {
            for (u32 x = 0; x < dim.x; ++x)
            {
                firstPassRays.emplace_back(generateRay(glm::uvec2(x, y), camera, dim));
            }
        }

        u32        firstPassWorkLoad = firstPassRays.size();
        u32        threadWorkload = firstPassWorkLoad / workGroupCount;
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = threadWorkload * i;
            u32 end = min(start + threadWorkload, firstPassWorkLoad);
            workers[i] = new std::thread(&PathTracer::fillIrradianceCache, this, std::cref(firstPassRays), start, end, std::ref(camera));
        }

        // let the main thread monitor the progress
        while (progressCounter.load() < firstPassWorkLoad)
        {
            printf("\r[Info] Debugging first pass of irradiance caching: Processed %d pixels ... %.2f%%", progressCounter.load(), ((f32)progressCounter.load() * 100.f / firstPassWorkLoad));
            fflush(stdout);
        }
        printf("\n");
        // reset the counter
        progressCounter = 0;
    }

    // irradiance caching
    void PathTracer::fastRenderWorker(u32 start, u32 end, Camera& camera, u32 totalNumRays)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        f32 pixelSize = 1.f / (f32)numPixelsInY;
        f32 subPixelSize = pixelSize / (f32)sppxCount;

        for (u32 i = start; i < end; ++i)
        {
            u32 py = i / numPixelsInX;
            u32 px = i % numPixelsInX;
            glm::vec2 pixelCenterCoord;
            pixelCenterCoord.x = ((f32)px / (f32)numPixelsInX) * 2.f - 1.f;
            pixelCenterCoord.y = ((f32)py / (f32)numPixelsInY) * 2.f - 1.f;

            // stratified sampling
            glm::vec3 color(0.f);
            for (u32 spx = 0; spx < sppxCount; ++spx)
            {
                for (u32 spy = 0; spy < sppyCount; ++spy)
                {
                    glm::vec2 subPixelOffset((f32)spx * subPixelSize, (f32)spy * subPixelSize);
                    // jitter the sub pixel samples
                    glm::vec2 jitter = glm::vec2(subPixelSize) * glm::vec2(uniformSampleZeroToOne(), uniformSampleZeroToOne());
                    subPixelOffset += jitter;
                    glm::vec2 sampleScreenCoord = pixelCenterCoord + subPixelOffset;
                    sampleScreenCoord.x *= aspectRatio;

                    glm::vec3 ro = camera.position;
                    glm::vec3 rd = normalize(camera.forward * camera.n
                        + camera.right * sampleScreenCoord.x * w
                        + camera.up * sampleScreenCoord.y * w);
                    auto hit = traceScene(ro, rd);
                    color += fastShadeSurface(hit, ro, rd, getHitMaterial(hit));
                    progressCounter.fetch_add(1);
                }
            }
            color /= (sppxCount * sppyCount);
            // post-processing
            {
                // tone mapping
                auto toneMappedColor = ACESFilm(color);
                // gamma correction
                auto finalColor = gammaCorrect(toneMappedColor, .4545f);
                setPixel(px, py, finalColor);
            }
        }
    }

    void PathTracer::fillIrradianceCache(const std::vector<Ray>& rays, u32 start, u32 end, Camera& camera)
    {
        for (u32 i = start; i < end; ++i)
        {
            auto ray = rays[i];
            auto hit = traceScene(ray.ro, ray.rd);
            if (hit.t > 0.f)
            {
                glm::vec3 hitPos = ray.ro + hit.t * ray.rd;
                glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPos);
                glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
                hitPos += EPSILON * normal;
#if 1
                // somehow this is faster than the code path below
                approximateDiffuseInterreflection(hitPos, normal, m_irradianceCacheCfg.kError);
#else
                f32 weight = 0.f;
                m_irradianceCache->m_octree->traverse([this, &weight, &hitPos, &normal](std::queue<OctreeNode*>& nodes, OctreeNode* node) {
                    // check all the records in current node
                    for (u32 i = 0; i < node->records.size(); ++i)
                    {
                        auto& record = m_irradianceCache->m_records[node->records[i]];
                        f32 wi = getIrradianceLerpWeight(hitPos, normal, record, m_irradianceCacheCfg.kError);
                        // exclude cached samples that are "in front of" p
                        f32 dp = dot(hitPos - record.position, (record.normal + normal) * .5f);
                        if (wi > 0.f && dp >= 0.f)
                        {
                            weight += wi;
                            // early exit as long as we found one valid record, we can skip the hemisphere sampling
                            return;
                        }
                    }
                    // search among childs
                    for (u32 i = 0; i < 8; ++i)
                    { 
                        auto child = node->childs[i];
                        if (child)
                        {
                            // recurse into this child if it has records that potentially overlaps 'p'
                            if (abs(hitPos.x - child->center.x) < child->sideLength 
                                && abs(hitPos.y - child->center.y) < child->sideLength 
                                && abs(hitPos.z - child->center.z) < child->sideLength)
                            {
                                nodes.push(child);
                            }
                        }
                    }
                });
                if (weight <= 0.f)
                {
                    sampleIrradianceRecord(hitPos, normal);
                }
#endif
            }
            progressCounter.fetch_add(1);
        }
    }

    void PathTracer::fastRenderScene(Camera& camera)
    {
        Cyan::Toolkit::ScopedTimer timer("PathTracer::fastRenderScene", true);
        u32 numPixels = numPixelsInX * numPixelsInY;
        u32 totalNumRays = numPixels * sppxCount * sppyCount;
        const u32 workGroupCount = 1;
        u32 workGroupPixelCount = numPixels / workGroupCount;

        // divide work equally among threads
        std::thread* workers[workGroupCount] = { };

        // first pass fill-in irradiance records
        std::vector<Ray> firstPassRays;
        glm::uvec2 dim(numPixelsInX / 2, numPixelsInY / 2);
        for (u32 y = 0; y < dim.y; ++y)
        {
            for (u32 x = 0; x < dim.x; ++x)
            {
                firstPassRays.emplace_back(generateRay(glm::uvec2(x, y), camera, dim));
            }
        }

        u32        firstPassWorkLoad = firstPassRays.size();
        u32        threadWorkload = firstPassWorkLoad / workGroupCount;
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = threadWorkload * i;
            u32 end = min(start + threadWorkload, firstPassWorkLoad);
            workers[i] = new std::thread(&PathTracer::fillIrradianceCache, this, std::cref(firstPassRays), start, end, std::ref(camera));
        }

        // let the main thread monitor the progress
        while (progressCounter.load() < firstPassWorkLoad)
        {
            printf("\r[Info] First pass irradiance caching: Processed %d pixels ... %.2f%%", progressCounter.load(), ((f32)progressCounter.load() * 100.f / firstPassWorkLoad));
            fflush(stdout);
        }
        printf("\n");
        // reset the counter
        progressCounter = 0;

        // second pass
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = workGroupPixelCount * i;
            u32 end = min(start + workGroupPixelCount, numPixels);
            workers[i] = new std::thread(&PathTracer::fastRenderWorker, this, start, end, std::ref(camera), totalNumRays);
        }

        while (progressCounter.load() < totalNumRays)
        {
            printf("\r[Info] Traced %d rays ... %.2f%%", progressCounter.load(), ((f32)progressCounter.load() * 100.f / totalNumRays));
            fflush(stdout);
        }

        for (u32 i = 0; i < workGroupCount; ++i)
            workers[i]->join();
        printf("\n");

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // free resources
        progressCounter = 0;
        for (u32 i = 0; i < workGroupCount; ++i)
            delete workers[i];
    }

    /*
    * Vanila weight formula presented in [WRC88] "A Ray Tracing Solution for Diffuse Interreflection"
    */
    inline f32 computeIrradianceRecordWeight0(const glm::vec3& p, const glm::vec3& pn, const IrradianceRecord& record, f32 error)
    {
        f32 distance = glm::length(p - record.position);
        return 1.f / ((distance / record.r) + sqrt(max(0.f, 1.0f - dot(pn, record.normal)))) - 1.f / error;
    }

    /*
    * Improved weight formula presented in [TL04] "An Approximate Global Illumination System for Computer Generated Films"
    * with a slight tweak by removing the times 2.0 in the translation error
    */
    inline f32 computeIrradianceRecordWeight1(const glm::vec3& p, const glm::vec3& pn, const IrradianceRecord& record, f32 error)
    {
        f32 accuracy = 1.f / error;
        f32 distance = glm::length(p - record.position);
        f32 translationalError = 2.f * distance / (record.r);
        f32 rotationalError = sqrt(max(0.f, 1.f - dot(pn, record.normal))) / sqrt(1.f - glm::cos(10.f / M_PI));
        return 1.f - accuracy * max(translationalError, rotationalError);
    }

    f32 getIrradianceLerpWeight(const glm::vec3& p, const glm::vec3& pn, const IrradianceRecord& record, f32 error)
    {
#if 1
        return computeIrradianceRecordWeight0(p, pn, record, error); 
#else
        return computeIrradianceRecordWeight1(p, pn, record, error);
#endif
    }

    /*
    * Find valid irradiance records and compute their interpolation weights in the irradiance cache naively by iterating through all the records
    */
    void lerpIrradianceSlow(glm::vec3& outIrrad, f32& outWeight, const IrradianceCache& cache, const glm::vec3& p, const glm::vec3& pn, f32 error)
    {
        // brute-force find all the valid record that can be used for extrapolation
        for (u32 i = 0; i < cache.m_numRecords; i++)
        {
            auto& record = cache.m_records[i];
            f32 wi = getIrradianceLerpWeight(p, pn, record, error);
            // exclude cached samples that are "in front of" p
            f32 dp = dot(p - record.position, (record.normal + pn) * .5f);
            if (wi > 0.f && dp >= 0.f)
            {
                // rotational gradient
                f32 rr = dot(glm::cross(record.normal, pn), record.gradient_r[0]);
                f32 rg = dot(glm::cross(record.normal, pn), record.gradient_r[1]);
                f32 rb = dot(glm::cross(record.normal, pn), record.gradient_r[2]);
                // translational gradient
                f32 tr = dot(p - record.position, record.gradient_t[0]);
                f32 tg = dot(p - record.position, record.gradient_t[1]);
                f32 tb = dot(p - record.position, record.gradient_t[2]);
                outIrrad += wi * (record.irradiance + glm::vec3(max(tr, 0.f), max(tg, 0.f), max(tb, 0.f)));
                outWeight += wi;
            }
        }
    }

    /*
    * Efficiently find valid irradiance records and compute their interpolation weights in the irradiance cache by traversing a Octree
    */
    void lerpIrradianceFast(glm::vec3& outIrrad, f32& outWeight, const IrradianceCache& cache, const glm::vec3& p, const glm::vec3& pn, f32 error)
    {
        cache.m_octree->traverse([&outIrrad, &outWeight, &cache, &p, &pn, error](std::queue<OctreeNode*>& nodes, OctreeNode* node) {
            // check all the records in current node
            for (u32 i = 0; i < node->records.size(); ++i)
            {
                auto& record = cache.m_records[node->records[i]];
                f32 wi = getIrradianceLerpWeight(p, pn, record, error);
                // exclude cached samples that are "in front of" p
                f32 dp = dot(p - record.position, (record.normal + pn) * .5f);
                if (wi > 0.f && dp >= 0.f)
                {
                    // rotational gradient
                    f32 rr = dot(glm::cross(record.normal, pn), record.gradient_r[0]);
                    f32 rg = dot(glm::cross(record.normal, pn), record.gradient_r[1]);
                    f32 rb = dot(glm::cross(record.normal, pn), record.gradient_r[2]);
                    // translational gradient
                    f32 tr = dot(p - record.position, record.gradient_t[0]);
                    f32 tg = dot(p - record.position, record.gradient_t[1]);
                    f32 tb = dot(p - record.position, record.gradient_t[2]);
                    glm::vec3 gradients(max(tr + 1.f, 0.f), max(tg + 1.f, 0.f), max(tb + 1.f, 0.f));
                    outIrrad += wi * (record.irradiance + glm::vec3(tr, tg, tb));
                    outWeight += wi;
                }
            }
            // search among childs
            for (u32 i = 0; i < 8; ++i)
            { 
                auto child = node->childs[i];
                if (child)
                {
                    // recurse into this child if it has records that potentially overlaps 'p'
                    if (abs(p.x - child->center.x) < child->sideLength 
                        && abs(p.y - child->center.y) < child->sideLength 
                        && abs(p.z - child->center.z) < child->sideLength)
                    {
                        nodes.push(child);
                    }
                }
            }
        });
    }

    /*
    * Compute indirect diffuse interreflection using irradiance caching
    */
    glm::vec3 PathTracer::approximateDiffuseInterreflection(glm::vec3& p, glm::vec3& pn, f32 error)
    {
        glm::vec3 irradiance(0.f);
        f32       weight = 0.f;
#if 0
        lerpIrradianceSlow(irradiance, weight, *m_irradianceCache, p, pn, error);
#else
        lerpIrradianceFast(irradiance, weight, *m_irradianceCache, p, pn, error);
#endif
        if (weight > 0.f)
        {
            irradiance /= weight;
        }
        else
        {
           irradiance = sampleIrradianceRecord(p, pn).irradiance;
        }
        return irradiance;
    }

    Octree::Octree()
        : m_numAllocatedNodes(0), m_root(nullptr)
    {
        m_nodePool.resize(maxNodeCount);
    }

    void Octree::init(const glm::vec3& center, f32 sideLength)
    {
        m_root = allocNode();
        m_root->center = center;
        m_root->sideLength = sideLength;
    }

    u32 Octree::getChildIndexEnclosingSurfel(OctreeNode* node, const glm::vec3& position)
    {
        glm::vec3 vv = position - node->center;
        u32 k = (vv.z <= 0.f) ? 1 : 0;
        u32 i = (vv.y <= 0.f) ? 1 : 0;
        u32 j = (vv.x >= 0.f) ? 1 : 0;
        return k * 4 + i * 2 + j;
    }

    OctreeNode* Octree::allocNode()
    {
        CYAN_ASSERT(m_numAllocatedNodes < maxNodeCount, "Too many OctreeNode allocated!"); return &m_nodePool[m_numAllocatedNodes++];
    }

    void Octree::traverse(const std::function<void(std::queue<OctreeNode*>&, OctreeNode*)>& callback)
    {
        std::queue<OctreeNode*> nodes;
        nodes.push(m_root);

        // breadth first search, loop termination is determined by 'callback'
        while (!nodes.empty())
        {
            OctreeNode* node = nodes.front();
            nodes.pop();

            callback(nodes, node);
        }
    }

    const static glm::vec3 childOffsets[8] = {
        glm::vec3(-.5f,  .5f,  .5f),  // 0
        glm::vec3( .5f,  .5f,  .5f),  // 1
        glm::vec3(-.5f, -.5f,  .5f),  // 2
        glm::vec3( .5f, -.5f,  .5f),  // 3
        glm::vec3(-.5f,  .5f, -.5f),  // 4
        glm::vec3( .5f,  .5f, -.5f),  // 5
        glm::vec3(-.5f, -.5f, -.5f),  // 6
        glm::vec3( .5f, -.5f, -.5f),  // 7
    };

    void Octree::insert(const IrradianceRecord& newRecord, u32 recordIndex)
    {
        traverse([this, &newRecord, recordIndex](std::queue<OctreeNode*>& nodes, OctreeNode* node) {
            // current node is too big for this new record, recurse into child and subdivide
            if ((node->sideLength > (4.f * newRecord.r)))
            {
                // compute which octant that current point belongs to
                u32 childIndex = getChildIndexEnclosingSurfel(node, newRecord.position);
                if (!node->childs[childIndex])
                {
                    node->childs[childIndex] = allocNode();
                    node->childs[childIndex]->sideLength = node->sideLength * .5f;
                    node->childs[childIndex]->center = node->center + childOffsets[childIndex] * node->childs[childIndex]->sideLength;
                }
                nodes.push(node->childs[childIndex]);
            }
            else if (node->sideLength >= (2.f * newRecord.r))
            {
                node->records.push_back(recordIndex);
            }
            else
                cyanError("Invalid record insertion into octree!");
        });
    }
 
    IrradianceCache::IrradianceCache()
        : m_numRecords(0u), m_octree(nullptr)
    {
        m_records.resize(cacheSize);
        m_numRecords = 0u;
        m_octree = new Octree();
    }

    void IrradianceCache::init(std::vector<SceneNode*>& nodes)
    {
        BoundingBox3D aabb;
        for (u32 i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i]->m_meshInstance)
            {
                auto& objectSpaceAABB = nodes[i]->m_meshInstance->getAABB();
                glm::mat4 model = nodes[i]->getWorldTransform().toMatrix();
                BoundingBox3D worldSpaceAABB;
                worldSpaceAABB.pmin = model * objectSpaceAABB.pmin;
                worldSpaceAABB.pmax = model * objectSpaceAABB.pmax;
                aabb.bound(worldSpaceAABB);
            }
        }
        f32 length = aabb.pmax.x - aabb.pmin.x;
        length = max(length, aabb.pmax.y - aabb.pmin.y);
        length = max(length, aabb.pmax.z - aabb.pmin.z);
        m_octree->init((aabb.pmin + aabb.pmax) * .5f, length);
    }

    const IrradianceRecord& IrradianceCache::addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r, const glm::vec3* rotationalGradient, const glm::vec3* translationalGradient)
    {
        CYAN_ASSERT(m_numRecords < cacheSize, "Too many irradiance records inserted!");
        IrradianceRecord& newRecord = m_records[m_numRecords];
        newRecord.position = p;
        newRecord.normal = pn;
        newRecord.irradiance = irradiance;
        newRecord.r = r;
        newRecord.gradient_r[0] = rotationalGradient[0];
        newRecord.gradient_r[1] = rotationalGradient[1];
        newRecord.gradient_r[2] = rotationalGradient[2];
        newRecord.gradient_t[0] = translationalGradient[0];
        newRecord.gradient_t[1] = translationalGradient[1];
        newRecord.gradient_t[2] = translationalGradient[2];
        // insert into the octree
        m_octree->insert(newRecord, m_numRecords++);
        return newRecord;
    }
#endif
};
