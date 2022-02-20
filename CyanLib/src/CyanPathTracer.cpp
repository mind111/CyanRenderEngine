#include <thread>
#include <queue>
#include <atomic>

#include "CyanPathTracer.h"
#include "Texture.h"
#include "MathUtils.h"
#include "CyanAPI.h"

/* 
    * todo: (Feature) integrate intel's open image denoiser
*/

// irradiance caching controls
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
#define EPSILON              0.002f

namespace Cyan
{
    f32 saturate(f32 val);
    Ray generateRay(const glm::uvec2& pixelCoord, Camera& camera, const glm::uvec2& screenDim);
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord);
    glm::vec3 ACESFilm(const glm::vec3& color);
    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma);
    inline f32 getLuminance(const glm::vec3& color);
    inline f32 getSpectralAverage(const glm::vec3& rgb);
    f32 getIrradianceLerpWeight(const glm::vec3& p, const glm::vec3& pn, IrradianceRecord* record, f32 error);

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

    // TODO: bilinear texture filtering
    glm::vec3 texelFetch(Texture* texture, glm::vec3& texCoord)
    {
        // how to interpret the data should be according to texture format
        u32 px = (u32)roundf(texCoord.x * texture->m_width);
        u32 py = (u32)roundf(texCoord.y * texture->m_height);
        // assuming rgb one byte per pixel
        u32 bytesPerPixel = 4;
        u32 flattendPixelIndex = (py * texture->m_width + px);
        u32 bytesOffset = flattendPixelIndex * bytesPerPixel;
        u8* pixelAddress = ((u8*)texture->m_data + bytesOffset);
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

    // return surface normal at ray hit in world space
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord)
    {
        auto mesh = rayHit.m_node->m_meshInstance->m_mesh;
        auto sm = mesh->m_subMeshes[rayHit.smIndex];
        u32 vertexOffset = rayHit.triIndex * 3;
        glm::vec3 normal = baryCoord.x * sm->m_triangles.m_normalArray[vertexOffset]
            + baryCoord.y * sm->m_triangles.m_normalArray[vertexOffset + 1]
            + baryCoord.z * sm->m_triangles.m_normalArray[vertexOffset + 2];
        normal = glm::normalize(normal);
        // convert normal back to world space
        auto worldTransformMatrix = rayHit.m_node->getWorldTransform().toMatrix();
        normal = vec4ToVec3(glm::inverse(glm::transpose(worldTransformMatrix)) * glm::vec4(normal, 0.f));
        return glm::normalize(normal);
    }

    PathTracer* PathTracer::m_singleton = nullptr;
    std::atomic<u32> PathTracer::progressCounter(0u);

    PathTracer::PathTracer()
        : m_scene(nullptr)
        , m_pixels(nullptr)
        , m_texture(nullptr)
        , m_skyColor(0.328f, 0.467f, 1.f)
        , m_renderMode(RenderMode::Render)
        , m_irradianceCache(nullptr)
        , m_irradianceCacheLevels{ nullptr, nullptr, nullptr }
        , m_debugPos0(0.f)
    {
        if (!m_singleton)
        {
            u32 bytesPerPixel = sizeof(float) * 3;
            u32 numPixels = numPixelsInX * numPixelsInY;
            m_pixels = (float*)new char[(u64)(bytesPerPixel * numPixels)];
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_width = numPixelsInX;
            spec.m_height = numPixelsInY;
            spec.m_format = Texture::ColorFormat::R32G32B32;
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_dataType = Texture::DataType::Float;
            m_texture = textureManager->createTexture("PathTracingOutput", spec);
            m_singleton = this;
            m_irradianceCache = new IrradianceCache;
            for (u32 i = 0; i < 3; ++i)
                m_irradianceCacheLevels[i] = new IrradianceCache;
        }
        else
            cyanError("Multiple instances are created for PathTracer!");
    }

    PathTracer* PathTracer::getSingletonPtr()
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
        m_irradianceCache->initialize(m_staticMeshNodes);
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

        switch (m_renderMode)
        {
            case RenderMode::Render:
#ifdef USE_IC
                fastRenderScene(camera);
#else
                renderSceneMultiThread(camera);
#endif
                break;
            default:
                printf("Invalid PathTracer setting! \n");
                break;
        }
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
            props.normal = baryCoord.x * sm->m_triangles.m_normalArray[vertexOffset]
                        + baryCoord.y * sm->m_triangles.m_normalArray[vertexOffset + 1]
                        + baryCoord.z * sm->m_triangles.m_normalArray[vertexOffset + 2];
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
            glm::mat4 model = m_staticMeshNodes[i]->getWorldMatrix();
            BoundingBox3f aabb = m_staticMeshNodes[i]->m_meshInstance->getAABB();

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
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
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
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
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

    void computeIrradianceGradients(IrradianceRecord& record, HemisphereCellData* cells, f32 M, f32 N)
    {

    }

    /* Notes:
    * It seems having separate gradient vector for each color component doesn't improve quality, maybe just use spectral average
    * mentioned in the original Irradiance Gradients paper instead ?
    * 
    * translational gradients still seems somewhat wrong, not sure which part is incorrect.
    */
    const IrradianceRecord& PathTracer::sampleIrradianceRecord(glm::vec3& p, glm::vec3& n)
    {
        glm::vec3 irradiance(0.f);

        // stratified sampling constants
        const f32 M = 32.f;
        const f32 N = 32.f;
        f32 numSamples = M * N;
        const f32 dTheta = .5f * M_PI / M;
        const f32 dPhi   = 2.f * M_PI / N;
        auto tangentFrame = tangentToWorld(n);
        auto tangent = tangentFrame[0];
        auto bitangent = tangentFrame[1];
        glm::vec3 gradient_r[3] = {
            glm::vec3(0.f),
            glm::vec3(0.f),
            glm::vec3(0.f)
        };
        glm::vec3 gradient_t[3] = {
            glm::vec3(0.f),
            glm::vec3(0.f),
            glm::vec3(0.f)
        };
        f32* prevSliceLr = static_cast<f32*>(alloca(sizeof(f32) * M));
        f32* prevSliceLg = static_cast<f32*>(alloca(sizeof(f32) * M));
        f32* prevSliceLb = static_cast<f32*>(alloca(sizeof(f32) * M));
        f32* prevSliceR = static_cast<f32*>(alloca(sizeof(f32) * M));
        glm::vec3 prevVk(0.f);
        for (u32 i = 0; i < M; ++i)
        {
            prevSliceR[i] = FLT_MAX;
            prevSliceLr[i] = 0.f;
            prevSliceLg[i] = 0.f;
            prevSliceLb[i] = 0.f;
        }

        f32 Ri = FLT_MAX;
        for (u32 k = 0; k < N; ++k)
        {
            f32 scale_t0[3] = { 0.f, 0.f, 0.f };
            f32 scale_t1[3] = { 0.f, 0.f, 0.f };

            // rotate tangent vector by (k + .5f) * dPhi to get a unit vector point towards center of cell j,k 
            auto quat = glm::angleAxis(dPhi * ((f32)k + .5f), n);
            glm::vec3 uk = glm::normalize(glm::rotate(quat, tangent));
            glm::vec3 vk = glm::normalize(glm::rotate(quat, bitangent));
            f32 prevThetaLr = 0.f;
            f32 prevThetaLg = 0.f;
            f32 prevThetaLb = 0.f;
            f32 prevThetaR = FLT_MAX;

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
                    if (dot(rd, n) >= 0.1f)
                        Ri = min(Ri, hit.t);
                    hitDistance = hit.t;
                }
                {
                    // translational gradient
                    f32 sineThetaMinus = sqrt((f32)j / M);
                    f32 sineThetaPlus = sqrt(((f32)j + 1.f) / M);

                    // todo: is clamping the motion rate here correct?
                    if (j > 0)
                    {
                        scale_t0[0] += sineThetaMinus * (1.f - sineThetaMinus * sineThetaMinus) * (radiance.r - prevThetaLr) / max(min(prevThetaR, hitDistance), 1.f);
                        scale_t0[1] += sineThetaMinus * (1.f - sineThetaMinus * sineThetaMinus) * (radiance.g - prevThetaLg) / max(min(prevThetaR, hitDistance), 1.f);
                        scale_t0[2] += sineThetaMinus * (1.f - sineThetaMinus * sineThetaMinus) * (radiance.b - prevThetaLb) / max(min(prevThetaR, hitDistance), 1.f);
                    }
                    if (k > 0)
                    {
                        scale_t1[0] += (sineThetaPlus - sineThetaMinus) * (radiance.r - prevSliceLr[j]) / max(min(prevSliceR[j], hitDistance), 1.f);
                        scale_t1[1] += (sineThetaPlus - sineThetaMinus) * (radiance.g - prevSliceLg[j]) / max(min(prevSliceR[j], hitDistance), 1.f);
                        scale_t1[2] += (sineThetaPlus - sineThetaMinus) * (radiance.b - prevSliceLb[j]) / max(min(prevSliceR[j], hitDistance), 1.f);
                    }
                    prevSliceLr[j] = radiance.r;
                    prevSliceLg[j] = radiance.g;
                    prevSliceLb[j] = radiance.b;
                    prevSliceR[j] = hitDistance;
                    prevThetaLr = radiance.r;
                    prevThetaLg = radiance.g;
                    prevThetaLb = radiance.b;
                    prevThetaR = hitDistance;
                }
            }
            gradient_t[0] += scale_t0[0] * (2 * M_PI / N) * uk;
            gradient_t[1] += scale_t0[1] * (2 * M_PI / N) * uk;
            gradient_t[2] += scale_t0[2] * (2 * M_PI / N) * uk;
            gradient_t[0] += scale_t1[0] * prevVk;
            gradient_t[1] += scale_t1[1] * prevVk;
            gradient_t[2] += scale_t1[2] * prevVk;
            prevVk = vk;
        };
        irradiance /= numSamples;

        // clamp sample spacing to avoid too sparse or too dense sample distribution
        Ri = glm::clamp(Ri, 0.1f, 2.0f);

        // debugging (translational gradients should be on the tangent plane of the hemisphere sample)
        m_debugData.translationalGradients.push_back(gradient_t[0]);
        // add to the cache
        return m_irradianceCache->addIrradianceRecord(p, n, irradiance, Ri, gradient_r, gradient_t);
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
            m_debugObjects.octreeBoundingBoxes.emplace_back();
            auto& aabb = m_debugObjects.octreeBoundingBoxes.back();
            auto& octreeNode = m_irradianceCache->m_octree->m_nodePool[i];
            aabb.m_pMin = glm::vec4(octreeNode.center + glm::vec3(-.5f, -.5f, -.5f) * octreeNode.sideLength, 1.f);
            aabb.m_pMax = glm::vec4(octreeNode.center + glm::vec3( .5f,  .5f,  .5f) * octreeNode.sideLength, 1.f);
            aabb.init();
            glm::mat4 model(1.f);
            aabb.setModel(model);
        }
        // generate debug spheres for cached irradiance samples
        cyanInfo("There were %u cached irradiance records", m_irradianceCache->m_numRecords);
        for (u32 i = 0; i < m_irradianceCache->m_numRecords; ++i)
        {
            m_debugObjects.debugSpheres.emplace_back();
            auto& sphere = m_debugObjects.debugSpheres.back();
            sphere.center = m_irradianceCache->m_cache[i].position;
            sphere.radius = m_irradianceCache->m_cache[i].r;
        }
#endif
        u32 px = 320, py = 240;
        auto ray0 = generateRay(glm::uvec2(px, py), camera, glm::uvec2(numPixelsInX, numPixelsInY));
        auto debugHit0 = traceScene(ray0.ro, ray0.rd);
        m_debugPos0 = ray0.ro + debugHit0.t * ray0.rd;

        // compute barycentric coord of the surface point that we hit 
        glm::vec3 baryCoord  = computeBaryCoordFromHit(debugHit0, m_debugPos0);
        glm::vec3 normal     = getSurfaceNormal(debugHit0, baryCoord);
        m_debugPos0 += normal * EPSILON;
        m_debugData.debugIrradianceRecord = IrradianceRecord(sampleIrradianceRecord(m_debugPos0, normal));
        /*
        auto tangentFrame = tangentToWorld(normal);
        u32 numDebugTangentFrames = sizeof(m_debugData.hemisphereTangentFrame) / sizeof(m_debugData.hemisphereTangentFrame[0]);
        for (u32 i = 0; i < numDebugTangentFrames; ++i)
        {
            glm::quat rotation = glm::angleAxis((f32)i * (2 * M_PI) / (f32)(numDebugTangentFrames), normal);
            m_debugData.hemisphereTangentFrame[i * 2 + 0] = glm::rotate(rotation, tangentFrame[0]);
            m_debugData.hemisphereTangentFrame[i * 2 + 1] = glm::rotate(rotation, tangentFrame[1]);
        }
        */
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
#ifdef DEBUG_IC
                m_debugData.debugRayHitPositions.push_back(ray.ro + ray.rd * hit.t);
#endif
                glm::vec3 hitPos = ray.ro + hit.t * ray.rd;
                glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPos);
                glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
                hitPos += EPSILON * normal;
                approximateDiffuseInterreflection(hitPos, normal, m_irradianceCacheCfg.kError);
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
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
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
    inline f32 computeIrradianceRecordWeight0(const glm::vec3& p, const glm::vec3& pn, IrradianceRecord* record, f32 error)
    {
        f32 distance = glm::length(p - record->position);
        return 1.f / ((distance / record->r) + sqrt(max(0.f, 1.0f - dot(pn, record->normal)))) - 1.f / error;
    }

    /*
    * Improved weight formula presented in [TL04] "An Approximate Global Illumination System for Computer Generated Films"
    * with a slight tweak by removing the times 2.0 in the translation error
    */
    inline f32 computeIrradianceRecordWeight1(const glm::vec3& p, const glm::vec3& pn, IrradianceRecord* record, f32 error)
    {
        f32 accuracy = 1.f / error;
        f32 distance = glm::length(p - record->position);
        f32 translationalError = 2.f * distance / (record->r);
        f32 rotationalError = sqrt(max(0.f, 1.f - dot(pn, record->normal))) / sqrt(1.f - glm::cos(10.f / M_PI));
        return 1.f - accuracy * max(translationalError, rotationalError);
    }

    f32 getIrradianceLerpWeight(const glm::vec3& p, const glm::vec3& pn, IrradianceRecord* record, f32 error)
    {
#if 0
        return computeIrradianceRecordWeight0(p, pn, record, error); 
#else
        return computeIrradianceRecordWeight1(p, pn, record, error);
#endif
    }

    /*
    * Compute indirect diffuse interreflection using irradiance caching
    */
    glm::vec3 PathTracer::approximateDiffuseInterreflection(glm::vec3& p, glm::vec3& pn, f32 error)
    {
        glm::vec3 irradiance(0.f);
        f32       weight = 0.f;
        for (u32 i = 0; i < m_irradianceCache->m_numRecords; i++)
        {
            auto& record = m_irradianceCache->m_records[i];
            f32 wi = getIrradianceLerpWeight(p, pn, &record, error);
            // exclude cached samples that are "in front of" p
            f32 dp = dot(p - record.position, (record.normal + pn) * .5f);
            if (wi > 0.f && dp >= 0.f)
            {
#if 1
                // rotational gradient
                f32 rr = dot(glm::cross(record.normal, pn), record.gradient_r[0]);
                f32 rg = dot(glm::cross(record.normal, pn), record.gradient_r[1]);
                f32 rb = dot(glm::cross(record.normal, pn), record.gradient_r[2]);
                // translational gradient
                f32 tr = dot(p - record.position, record.gradient_t[0]);
                f32 tg = dot(p - record.position, record.gradient_t[1]);
                f32 tb = dot(p - record.position, record.gradient_t[2]);
                irradiance += wi * (record.irradiance + glm::vec3(tr, tg, tb));
#else
                irradiance += wi * record.irradiance;
#endif
                weight += wi;
            }
        }
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

    glm::vec3 childOffsets[8] = {
        glm::vec3(-.5f,  .5f,  .5f),  // 0
        glm::vec3( .5f,  .5f,  .5f),  // 1
        glm::vec3(-.5f, -.5f,  .5f),  // 2
        glm::vec3( .5f, -.5f,  .5f),  // 3
        glm::vec3(-.5f,  .5f, -.5f),  // 4
        glm::vec3( .5f,  .5f, -.5f),  // 5
        glm::vec3(-.5f, -.5f, -.5f),  // 6
        glm::vec3( .5f, -.5f, -.5f),  // 7
    };

    void Octree::insert(IrradianceRecord* newRecord)
    {
        std::queue<OctreeNode*> nodes;
        nodes.push(m_root);

        // breadth first search
        while (!nodes.empty())
        {
            OctreeNode* node = nodes.front();
            nodes.pop();
            // recurse into child
            if (node->sideLength > 0.1f && (node->sideLength > (4.f * newRecord->r)))
            {
                // compute which octant that current point belongs to
                u32 childIndex = getChildIndexEnclosingSurfel(node, newRecord->position);
                if (!node->childs[childIndex])
                {
                    node->childs[childIndex] = allocNode();
                    node->childs[childIndex]->sideLength = node->sideLength * .5f;
                    node->childs[childIndex]->center = node->center + childOffsets[childIndex] * node->childs[childIndex]->sideLength;
                    nodes.push(node->childs[childIndex]);
                }
            }
            else
                node->records.push_back(newRecord);
        }
    }

    u32 Octree::getChildIndexEnclosingSurfel(OctreeNode* node, glm::vec3& position)
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
 
    IrradianceCache::IrradianceCache()
        : m_numRecords(0u), m_octree(nullptr)
    {
        m_records.resize(cacheSize);
        m_numRecords = 0u;
        m_octree = new Octree();
    }

    void IrradianceCache::initialize(std::vector<SceneNode*>& nodes)
    {
        BoundingBox3f aabb;
        for (u32 i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i]->m_meshInstance)
            {
                auto& objectSpaceAABB = nodes[i]->m_meshInstance->getAABB();
                glm::mat4 model = nodes[i]->getWorldTransform().toMatrix();
                BoundingBox3f worldSpaceAABB;
                worldSpaceAABB.m_pMin = model * objectSpaceAABB.m_pMin;
                worldSpaceAABB.m_pMax = model * objectSpaceAABB.m_pMax;
                aabb.bound(worldSpaceAABB);
            }
        }
        f32 length = aabb.m_pMax.x - aabb.m_pMin.x;
        length = max(length, aabb.m_pMax.y - aabb.m_pMin.y);
        length = max(length, aabb.m_pMax.z - aabb.m_pMin.z);
        m_octree->init((aabb.m_pMin + aabb.m_pMax) * .5f, length);
    }

    const IrradianceRecord& IrradianceCache::addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r, const glm::vec3* rotationalGradient, const glm::vec3* translationalGradient)
    {
        CYAN_ASSERT(m_numRecords < cacheSize, "Too many irradiance records inserted!");
        IrradianceRecord* newRecord = &m_records[m_numRecords++];
        newRecord->position = p;
        newRecord->normal = pn;
        newRecord->irradiance = irradiance;
        newRecord->r = r;
        newRecord->gradient_r[0] = rotationalGradient[0];
        newRecord->gradient_r[1] = rotationalGradient[1];
        newRecord->gradient_r[2] = rotationalGradient[2];
        newRecord->gradient_t[0] = translationalGradient[0];
        newRecord->gradient_t[1] = translationalGradient[1];
        newRecord->gradient_t[2] = translationalGradient[2];
        // insert into the octree
        // m_octree->insert(newRecord);
        return *newRecord;
    }

    void IrradianceCache::findValidRecords(std::vector<IrradianceRecord*>& validSet, const glm::vec3& p, const glm::vec3& pn, f32 error)
    {
#if 0
        std::queue<OctreeNode*> nodes;
        nodes.push(m_octree->m_root);
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            for (u32 i = 0; i < node->records.size(); ++i)
            {
                f32 wi = getIrradianceLerpWeight(p, pn, node->records[i], kError);
                // exclude cached samples that are "in front of" p
                f32 dp = dot(p - node->records[i]->position, (node->records[i]->normal + pn) * .5f);
                if (wi > 0.f && dp >= 0.f) validSet.push_back(node->records[i]);
            }

            // recurse into child
            for (u32 i = 0; i < 8; ++i)
            {
                if (node->childs[i] && glm::length(p - node->childs[i]->center) <= node->childs[i]->sideLength)
                    nodes.push(node->childs[i]);
            }
        }
#else
        for (u32 i = 0; i < m_numRecords; i++)
        {
            auto& record = m_records[i];
            f32 wi = getIrradianceLerpWeight(p, pn, &record, error);
            // exclude cached samples that are "in front of" p
            f32 dp = dot(p - record.position, (record.normal + pn) * .5f);
            if (wi > 0.f && dp >= 0.f)
                validSet.push_back(&record);
        }
#endif
    }
};
