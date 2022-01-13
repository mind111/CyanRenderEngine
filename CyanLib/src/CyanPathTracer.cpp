#include <thread>
#include <queue>
#include <atomic>

#include "CyanPathTracer.h"
#include "Texture.h"
#include "MathUtils.h"
#include "CyanAPI.h"

/* 
    * todo: (Feature) integrate intel's open image denoiser
    * todo: (Optimization) think about how can I further improve performance
    * todo: treat the sun light as a disk area light source
*/

// Is 2mm of an offset too large..?
#define EPSILON 0.002f
#define POST_PROCESSING 1
#define PATHTRACE_IC 1
#define RECURSIVE_IC 0
#define VISUALIZE_IRRADIANCE 0

namespace Cyan
{
    f32 saturate(f32 val);
    glm::vec3 ACESFilm(const glm::vec3& color);
    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma);
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord);

    PathTracer* PathTracer::m_singleton = nullptr;
    std::atomic<u32> PathTracer::progressCounter(0u);

    PathTracer::PathTracer()
        : m_scene(nullptr)
        , m_pixels(nullptr)
        , m_texture(nullptr)
        , m_skyColor(0.328f, 0.467f, 1.f)
        , m_renderMode(RenderMode::Render)
        , m_irradianceCache(nullptr)
        , m_irradianceCaches{ nullptr, nullptr, nullptr }
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
                m_irradianceCaches[i] = new IrradianceCache;
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
        m_irradianceCache->init(m_staticSceneNodes);
    }

    glm::vec2 getScreenCoordOfPixel(u32 x, u32 y)
    {
        return glm::vec2(0.f);
    }

    void PathTracer::setPixel(u32 px, u32 py, const glm::vec3& color)
    {
        u32 index = (py * numPixelsInX + px) * numChannelPerPixel;
        CYAN_ASSERT(index < numPixelsInX* numPixelsInY* numChannelPerPixel, "Writing to a pixel out of bound");
        m_pixels[index + 0] = color.r;
        m_pixels[index + 1] = color.g;
        m_pixels[index + 2] = color.b;
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
#if VISUALIZE_IRRADIANCE
                        glm::vec3 hitPosition = ro + rd * hit.t;
                        auto mesh = hit.m_node->m_meshInstance->m_mesh;
                        auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex);
                        auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;
                        // compute barycentric coord of the surface point that we hit 
                        glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
                        glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
                        hitPosition += normal * EPSILON;
                        color += sampleNewIrradianceRecord(hitPosition, normal) * getHitMaterial(hit).flatColor;
#else
                        color += renderSurface(hit, ro, rd, getHitMaterial(hit));
#endif
                    }
                    progressCounter.fetch_add(1);
                }
            }
            color /= (sppxCount * sppyCount);
#if POST_PROCESSING
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
                            pixelColor += renderSurface(hit, ro, rd, getHitMaterial(hit));

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

    f32 PathTracer::traceShadowRay(const glm::vec3& ro, glm::vec3& rd)
    {
        f32 shadow = 1.f;
        if (m_scene->castVisibilityRay(ro, rd, EntityFilter::BakeInLightMap))
            shadow = 0.f;
        return shadow;
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

    glm::vec3 PathTracer::computeDirectLighting(glm::vec3& hitPosition, glm::vec3& normal)
    {
        glm::vec3 color(0.f);
        for (auto dirLight : m_scene->dLights)
        {
            glm::vec3 lightDir = dirLight.direction;
            // trace shadow ray
            f32 shadow = traceShadowRay(hitPosition, lightDir);
            f32 ndotl = max(glm::dot(normal, lightDir), 0.f);
            glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
            color += Li * ndotl;
        }
        return color;
    }

    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma)
    {
        return glm::vec3(pow(color.r, gamma), pow(color.g, gamma), pow(color.b, gamma));
    }

    glm::vec3 PathTracer::computeDirectSkyLight(glm::vec3& ro, glm::vec3& n)
    {
        // TODO: sample skybox instead of using a constant sky color
        glm::vec3 outColor(0.f);
        const u32 sampleCount = 1u;
        for (u32 i = 0u; i < sampleCount; ++i)
        {
            glm::vec3 sampleDir = uniformSampleHemiSphere(n);
            // the ray hit the skybox
            if (!m_scene->castVisibilityRay(ro, sampleDir, EntityFilter::BakeInLightMap))
                outColor += m_skyColor * max(glm::dot(sampleDir, n), 0.f);
        }
        return outColor / (f32)sampleCount;
    }

    glm::vec3 PathTracer::recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces, TriMaterial& matl)
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

        glm::vec3 rd = cosineWeightedSampleHemiSphere(n);

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
            exitRadiance += (indirectBoost * recursiveTraceDiffuse(nextBounceRo, nextBounceNormal, numBounces - 1, nextMatl));
        }
        return exitRadiance * matl.flatColor;
    }

    glm::vec3 PathTracer::sampleNewIrradianceRecord(glm::vec3& p, glm::vec3& n)
    {
        const u32 numSamples = 128u;
        glm::vec3 irradiance(0.f);
        // harmonic mean of distance to visible surfaces and not set it to 0 to avoid divide by 0
        f32 r = 0.f;
        for (u32 i = 0; i < numSamples; ++i)
        {
            auto rd = cosineWeightedSampleHemiSphere(n);
            auto hit = traceScene(p, rd);
            if (hit.t > 0.f)
            {
                auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                auto radiance = renderSurface(hit, p, rd, matl);
                irradiance += radiance;
                r += (1.f / hit.t);
            }
        }
        irradiance /= numSamples;
        r = numSamples / r;
        r = glm::clamp(r, 0.1f, 8.0f);

        // add to the cache
        m_irradianceCache->addIrradianceRecord(p, n, irradiance, r);
        return irradiance;
    }

    glm::vec3 PathTracer::fastRenderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl)
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
            if (glm::dot(normal, rd) > 0) return radiance;

            // direct + indirect (diffuse interreflection)
            // radiance += computeDirectLighting(hitPosition, normal);
            // radiance += computeDirectSkyLight(hitPosition, normal);
#if PATHTRACE_IC
            radiance += irradianceCaching(hitPosition, normal, m_irradianceCache->kError * .2f);
#endif
#if RECURSIVE_IC
            radiance += recursiveIC(hitPosition, normal, getHitMaterial(hit), 0);
#endif
        }
        return radiance * matl.flatColor;
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

    void PathTracer::debugIC(Camera& camera)
    {
        u32 px = 64, py = 120;
        auto ray0 = generateRay(glm::uvec2(px, py), camera, glm::uvec2(numPixelsInX, numPixelsInY));
        auto debugHit0 = traceScene(ray0.ro, ray0.rd);
        m_debugPos0 = ray0.ro + debugHit0.t * ray0.rd;

        // compute barycentric coord of the surface point that we hit 
        glm::vec3 baryCoord  = computeBaryCoordFromHit(debugHit0, m_debugPos0);
        glm::vec3 normal     = getSurfaceNormal(debugHit0, baryCoord);
        cachedIrradiance = sampleNewIrradianceRecord(m_debugPos0, normal);

        u32 debugPx = 67, debugPy = 120;
        auto ray1 = generateRay(glm::uvec2(debugPx, debugPy), camera, glm::uvec2(numPixelsInX, numPixelsInY));
        auto debugHit1 = traceScene(ray1.ro, ray1.rd);
        m_debugPos1 = ray1.ro + debugHit1.t * ray1.rd;
        glm::vec3 baryCoord1  = computeBaryCoordFromHit(debugHit1, m_debugPos0);
        glm::vec3 normal1     = getSurfaceNormal(debugHit1, baryCoord);
        interpolatedIrradiance = irradianceCaching(m_debugPos1, normal1, m_irradianceCache->kError);
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
                    color += fastRenderSurface(hit, ro, rd, getHitMaterial(hit));
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

    void PathTracer::firstPassIC(const std::vector<Ray>& rays, u32 start, u32 end, Camera& camera)
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
#if PATHTRACE_IC
                //sampleNewIrradianceRecord(hitPos, normal);
                irradianceCaching(hitPos, normal, m_irradianceCache->kError);
#endif
#if RECURSIVE_IC
                recursiveIC(hitPos, normal, getHitMaterial(hit), 0);
#endif
            }
            progressCounter.fetch_add(1);
        }
    }

    void PathTracer::fastRenderScene(Camera& camera)
    {
        Cyan::Toolkit::ScopedTimer timer("PathTracer::renderSceneMultiThread", true);
        u32 numPixels = numPixelsInX * numPixelsInY;
        u32 totalNumRays = numPixels * sppxCount * sppyCount;
#if 1
        // trace all the rays using multi-threads
        // const u32 workGroupCount = 16;
        const u32 workGroupCount = 1;
        u32 workGroupPixelCount = numPixels / workGroupCount;

        // divide work into 16 threads
        std::thread* workers[workGroupCount] = { };

        // first pass fill-in irradiance records
        std::vector<Ray> firstPassRays;
        glm::uvec2 dim(numPixelsInX / 16, numPixelsInY / 16);
        for (u32 i = 0; i < 4; ++i)
        {
            for (u32 y = 0; y < dim.y; ++y)
            {
                for (u32 x = 0; x < dim.x; ++x)
                    firstPassRays.emplace_back(generateRay(glm::uvec2(x, y), camera, dim));
            }
            dim *= 2u;
        }
        u32        firstPassWorkLoad = firstPassRays.size();
        u32        threadWorkload = firstPassWorkLoad / workGroupCount;
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = threadWorkload * i;
            u32 end = min(start + threadWorkload, firstPassWorkLoad);
            workers[i] = new std::thread(&PathTracer::firstPassIC, this, std::cref(firstPassRays), start, end, std::ref(camera));
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
#else
        debugIC(camera);
#endif
    }

    void PathTracer::debugRender()
    {

    }

    inline f32 computeICWeight(const glm::vec3& p, const glm::vec3 pn, IrradianceRecord* record, f32 error) 
    {
        f32 distance = glm::length(p - record->position);
        return 1.f / ((distance / record->r) + sqrt(1.0f - min(glm::dot(pn, record->normal), 1.f))) - error;
    }

    // todo: instead of using cosine weighted importance sampling, do stratified hemisphere sampling!
    glm::vec3 PathTracer::recursiveIC(glm::vec3& p, glm::vec3& pn, const TriMaterial& matl, u32 level)
    {
        const u32 numSamples = 8u;
        glm::vec3 irradiance(0.f);

        auto ro = p + EPSILON * pn;
        if (level == numIndirectBounce)
        {
            // sample direct lighting
            glm::vec3 directLighting(0.f);
            for (u32 i = 0; i < 32; ++i)
                directLighting += computeDirectSkyLight(ro, pn);
            directLighting /= 32.f;
            directLighting += computeDirectLighting(ro, pn);
            return directLighting * matl.flatColor;
        }

        auto cache = m_irradianceCaches[level];
        std::vector<IrradianceRecord*> validSet;
        cache->findValidRecords(validSet, p, pn, cache->kError);
        if (validSet.empty())
        {
            // hemi-sphere sampling recursively
            f32 r = 0.f;
            for (u32 i = 0; i < numSamples; ++i)
            {
                auto rd = cosineWeightedSampleHemiSphere(pn);
                auto hit = traceScene(ro, rd);
                if (hit.t > 0.f)
                {
                    auto nextRo = ro + hit.t * rd;
                    glm::vec3 baryCoord = computeBaryCoordFromHit(hit, nextRo);
                    glm::vec3 normal = getSurfaceNormal(hit, baryCoord);
                    irradiance += recursiveIC(nextRo, normal, getHitMaterial(hit), level + 1);
                    r += (1.f / hit.t);
                }
            }
            irradiance /= numSamples;
            r = glm::clamp(numSamples / r, 0.1f, 2.f);
            cache->addIrradianceRecord(p, pn, irradiance, r);
            irradiance *= matl.flatColor;
        }
        else
        {
            f32 denominator = 0.f;
            for (u32 i = 0; i < validSet.size(); ++i)
            {
                auto validRecord = validSet[i];
                f32 distance = glm::length(p - validRecord->position);
                f32 wi = computeICWeight(p, pn, validRecord, m_irradianceCache->kError);
                irradiance += wi * validRecord->irradiance;
                denominator += wi;
            }
            irradiance /= denominator;
        }
        return irradiance;
    }

    glm::vec3 PathTracer::irradianceCaching(glm::vec3& p, glm::vec3& pn, f32 error)
    {
        // find valid set
        glm::vec3 irradiance(0.f);
        std::vector<IrradianceRecord*> validSet;
        m_irradianceCache->findValidRecords(validSet, p, pn, error);
        // compute irradiance
        if (validSet.empty())
        {
            irradiance = sampleNewIrradianceRecord(p, pn);
        }
        // "lazy" evaluation
        else
        {
            f32 denominator = 0.f;
            for (u32 i = 0; i < validSet.size(); ++i)
            {
                auto validRecord = validSet[i];
                f32 distance = glm::length(p - validRecord->position);
                f32 wi = computeICWeight(p, pn, validRecord, m_irradianceCache->kError);
                irradiance += wi * validRecord->irradiance;
                denominator += wi;
            }
            irradiance /= denominator;
        }
        return irradiance;
    }

    void PathTracer::preprocessSceneData()
    {
        // todo: deduplicate material instances
        u32 matlInstanceCount = 0;
        for (u32 i = 0; i < (u32)m_scene->entities.size(); ++i)
        {
            if (m_scene->entities[i]->m_static)
            {
                m_staticEntities.push_back(m_scene->entities[i]);
                std::queue<SceneNode*> nodes;
                nodes.push(m_scene->entities[i]->m_sceneRoot);
                while (!nodes.empty())
                {
                    auto* node = nodes.front();
                    nodes.pop();
                    if (node->m_meshInstance)
                    {
                        m_staticSceneNodes.push_back(node);
                        for (u32 sm = 0; sm < node->m_meshInstance->m_mesh->numSubMeshes(); ++sm)
                        {
                            node->m_meshInstance->m_rtMatls.emplace_back(matlInstanceCount++);
                            m_sceneMaterials.emplace_back();
                            auto& matl = m_sceneMaterials.back();
                            f32 hasDiffuseMap = node->m_meshInstance->m_matls[sm]->getF32("uMaterialProps.hasDiffuseMap");
                            if (hasDiffuseMap < .5f)
                            {
                                // assume that the input color value is in sRGB space
                                matl.flatColor = gammaCorrect(vec4ToVec3(node->m_meshInstance->m_matls[sm]->getVec4("flatColor")), 2.2f);
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
        switch (m_renderMode)
        {
            case RenderMode::Render:
#if 0
                renderScene(camera);
#else
                // renderSceneMultiThread(camera);
                fastRenderScene(camera);
#endif
                break;
            default:
                printf("Invalid PathTracer setting! \n");
                break;
        }
    }

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

    TriMaterial& PathTracer::getHitMaterial(RayCastInfo& hit)
    {
        return m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
    }

    glm::vec3 PathTracer::renderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd, TriMaterial& matl)
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
        if (glm::dot(normal, rd) > 0) return radiance;

        // direct + indirect (diffuse interreflection)
        radiance += recursiveTraceDiffuse(hitPosition, normal, numIndirectBounce - 1, getHitMaterial(hit));
        //radiance *= sampleAo(hitPosition, normal, 8);
        return radiance;
    }

    void PathTracer::postProcess()
    {
        // not implemented
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
            if (glm::dot(normal, rd) > 0) return radiance;

            // bake direct static sky light & indirect lighting
            glm::vec3 indirectRo = hitPosition + EPSILON * normal;
            radiance += computeDirectSkyLight(indirectRo, normal);
            radiance += recursiveTraceDiffuse(indirectRo, normal, 0, getHitMaterial(hit));
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
        // it seems two approaches have similar performance
#if 0
        for (u32 i = 0; i < m_staticEntities.size(); ++i)
        {
            auto hit = m_staticEntities[i]->intersectRay(ro, rd, glm::mat4(1.f));
            if (hit.t > 0.f && hit < closestHit)
                closestHit = hit;
        }
#else
        for (u32 i = 0; i < m_staticSceneNodes.size(); ++i)
        {
            glm::mat4 modelView = m_staticSceneNodes[i]->m_worldTransform.toMatrix();
            BoundingBox3f aabb = m_staticSceneNodes[i]->m_meshInstance->getAABB();

            // transform the ray into object space
            glm::vec3 roObjectSpace = ro;
            glm::vec3 rdObjectSpace = rd;
            transformRayToObjectSpace(roObjectSpace, rdObjectSpace, modelView);
            rdObjectSpace = glm::normalize(rdObjectSpace);

            if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = m_staticSceneNodes[i]->m_meshInstance->m_mesh;
                Cyan::MeshRayHit currentRayHit = mesh->intersectRay(roObjectSpace, rdObjectSpace);

                if (currentRayHit.t > 0.f)
                {
                    // convert hit from object space back to world space
                    auto objectSpaceHit = roObjectSpace + currentRayHit.t * rdObjectSpace;
                    f32 currentWorldSpaceDistance = transformHitFromObjectToWorldSpace(objectSpaceHit, modelView, ro, rd);
                    if (currentWorldSpaceDistance < closestHit.t)
                    {
                        closestHit.t = currentWorldSpaceDistance;
                        closestHit.smIndex = currentRayHit.smIndex;
                        closestHit.triIndex = currentRayHit.triangleIndex;
                        closestHit.m_node = m_staticSceneNodes[i];
                    }
                }
            }
        }
#endif
        if (!closestHit.m_node)
            closestHit.t = -1.f;
        return closestHit;
    }

    f32 PathTracer::sampleAo(glm::vec3& samplePos, glm::vec3& n, u32 numSamples)
    {
        f32 visibility = 1.f;
        for (u32 i = 0; i < numSamples; ++i)
        {
            glm::vec3 sampleDir = cosineWeightedSampleHemiSphere(n);
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
        const u32 numSamples = 8;
        glm::vec3 irradiance(0.f);
        for (u32 i = 0; i < numSamples; ++i)
        {
            auto rd = uniformSampleHemiSphere(n);
            auto hit = traceScene(samplePos, rd);
            if (hit.t > 0.f)
            {
                auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                // todo: instead of only consider diffuse reflections along the path, also include indirect specular
                auto radiance = bakeSurface(hit, samplePos, rd, matl);
                irradiance += radiance * max(glm::dot(rd, n), 0.f) * (1.f / M_PI);
            }
            else
                irradiance += m_skyColor * max(glm::dot(rd, n), 0.f);
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
            auto rd = cosineWeightedSampleHemiSphere(n);
            auto hit = traceScene(samplePos, rd);
            if (hit.t > 0.f)
            {
                auto& matl = m_sceneMaterials[hit.m_node->m_meshInstance->m_rtMatls[hit.smIndex]];
                auto radiance = renderSurface(hit, samplePos, rd, matl);
                irradiance += radiance;
            }
            else
                irradiance += m_skyColor;
        }
        irradiance /= numSamples;
        return irradiance;
    }

    glm::vec3 computeRayDirectionFromTexCoord(Camera& camera, glm::vec2 texCoord)
    {
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        glm::vec3 rd = normalize(camera.forward * camera.n
                               + camera.right   * texCoord.x * w 
                               + camera.up      * texCoord.y * w);
        return rd;
    }

    void PathTracer::bakeIrradianceProbe(glm::vec3& probePos, glm::ivec2& resolution)
    {
        glm::vec3 cameraTargets[] = {
            {1.f, 0.f, 0.f},   // Right
            {-1.f, 0.f, 0.f},  // Left
            {0.f, 1.f, 0.f},   // Up
            {0.f, -1.f, 0.f},  // Down
            {0.f, 0.f, 1.f},   // Front
            {0.f, 0.f, -1.f},  // Back
        }; 
        glm::vec3 worldUps[] = {
            {0.f, -1.f, 0.f},   // Right
            {0.f, -1.f, 0.f},   // Left
            {0.f,  0.f, 1.f},    // Up
            {0.f,  0.f, -1.f},   // Down
            {0.f, -1.f, 0.f},   // Forward
            {0.f, -1.f, 0.f},   // Back
        };

        Camera camera = { };
        for (u32 f = 0; f < 6; ++f)
        {
            // configure camera
            {
                camera.position = probePos;
                camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
                camera.n = 0.1f;
                camera.f = 100.f;
                camera.fov = glm::radians(90.f);
                camera.lookAt = camera.position + cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
            }

            // compute irradiance
            for (u32 x = 0; x < (u32)resolution.x; ++x)
            {
                for (u32 y = 0; y < (u32)resolution.y; ++y)
                {
                    glm::vec2 texCoord(0.f);
                    texCoord.x = ((f32)x / (f32)resolution.x) * 2.f - 1.f;
                    texCoord.y = ((f32)y / (f32)resolution.y) * 2.f - 1.f;

                    glm::vec3 hemiSphereNormalDir = computeRayDirectionFromTexCoord(camera, texCoord);
                    glm::vec3 irradiance = sampleIrradiance(probePos, hemiSphereNormalDir);

                    // write to a cubemap texture at direction "hemiSphereNormalDir"
                    {

                    }
                }
            }
        }
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
        m_cache.resize(cacheSize);
        m_numRecords = 0u;
        m_octree = new Octree();
    }

    void IrradianceCache::init(std::vector<SceneNode*>& nodes)
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

    void IrradianceCache::addIrradianceRecord(const glm::vec3& p, const glm::vec3& pn, const glm::vec3& irradiance, f32 r)
    {
        CYAN_ASSERT(m_numRecords < cacheSize, "Too many irradiance records inserted!");
        IrradianceRecord* newRecord = &m_cache[m_numRecords++];
        newRecord->position = p;
        newRecord->normal = pn;
        newRecord->irradiance = irradiance;
        newRecord->r = r;
        // insert into the octree
        // m_octree->insert(newRecord);
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
                f32 wi = computeICWeight(p, pn, node->records[i], kError);
                // exclude cached samples that are "in front of" p
                f32 dp = glm::dot(p - node->records[i]->position, (node->records[i]->normal + pn) * .5f);
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
            auto& record = m_cache[i];
            f32 wi = computeICWeight(p, pn, &record, error);
            // exclude cached samples that are "in front of" p
            f32 dp = glm::dot(p - record.position, (record.normal + pn) * .5f);
            if (wi > 0.f && dp >= 0.f)
                validSet.push_back(&record);
        }
#endif
    }
};
