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

namespace Cyan
{
    glm::vec3 ACESFilm(const glm::vec3& color);
    glm::vec3 gammaCorrect(const glm::vec3& color, f32 gamma);

    PathTracer* PathTracer::m_singleton = nullptr;
    std::atomic<u32> PathTracer::progressCounter(0u);

    PathTracer::PathTracer()
    : m_scene(nullptr)
    , m_pixels(nullptr)
    , m_texture(nullptr)
    , m_skyColor(0.328f, 0.467f, 1.f)
    , m_renderMode(RenderMode::Render)
    {
        if (!m_singleton)
        {
            u32 bytesPerPixel = sizeof(float) * 3;
            u32 numPixels = numPixelsInX * numPixelsInY;
            m_pixels = (float*)new char[(u64)(bytesPerPixel * numPixels)];
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec = { }; 
            spec.m_type   = Texture::Type::TEX_2D;
            spec.m_width  = numPixelsInX;
            spec.m_height = numPixelsInY;
            spec.m_format = Texture::ColorFormat::R32G32B32;
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_dataType = Texture::DataType::Float;
            m_texture = textureManager->createTexture("PathTracingOutput", spec);
            m_singleton = this;
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
    }

    glm::vec2 getScreenCoordOfPixel(u32 x, u32 y)
    {
        return glm::vec2(0.f);
    }

    void PathTracer::setPixel(u32 px, u32 py, const glm::vec3& color)
    {
        u32 index = (py * numPixelsInX + px) * numChannelPerPixel;
        CYAN_ASSERT(index < numPixelsInX * numPixelsInY * numChannelPerPixel, "Writing to a pixel out of bound");
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
                                           + camera.right   * sampleScreenCoord.x * w 
                                           + camera.up      * sampleScreenCoord.y * w);
                    auto hit = traceScene(ro, rd);
                    color += renderSurface(hit, ro, rd, getHitMaterial(hit));
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
                                               + camera.right   * sampleScreenCoord.x * w 
                                               + camera.up      * sampleScreenCoord.y * w);

                        auto hit = traceScene(ro, rd);
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
        return glm::vec3((r/255.f), (g/255.f), (b/255.f));
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

        // stop recursion
        if (numBounces >= 3)
            return exitRadiance;

        glm::vec3 rd = cosineWeightedSampleHemiSphere(n);

        // ray cast
        auto nextRayHit = m_scene->castRay(ro, rd, EntityFilter::BakeInLightMap);

        if (nextRayHit.t > 0.f)
        {
            // compute geometric information
            glm::vec3 nextBounceRo = ro + nextRayHit.t * rd;
            glm::vec3 baryCoord = computeBaryCoordFromHit(nextRayHit, nextBounceRo);

            glm::vec3 nextBounceNormal = getSurfaceNormal(nextRayHit, baryCoord);

            // avoid self-intersecting
            nextBounceRo += EPSILON * nextBounceNormal;
 
            // indirect
            f32 indirectBoost = 1.0f;

            // exitRadiance += (indirectBoost * recursiveTraceDiffuse(nextBounceRo, nextBounceNormal, numBounces + 1, matl) * max(glm::dot(n, rd), 0.f));
            // the cosine geometric term is cancelled with the pdf
            exitRadiance += (indirectBoost * recursiveTraceDiffuse(nextBounceRo, nextBounceNormal, numBounces + 1, matl));
        }
        return exitRadiance * (matl.flatColor * INV_PI);
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
                renderSceneMultiThread(camera);
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
            radiance += recursiveTraceDiffuse(hitPosition, normal, 0, getHitMaterial(hit));
            radiance *= sampleAo(hitPosition, normal, 8);
        }
        return radiance;
    }

    void PathTracer::postProcess()
    {
        // not implemented
    }

    // todo: make diffuse lambert reflectance into account when bouncing indirect light
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
            // todo: debug the cause of invalid baryCoord and a weird "black" line in the final render, it seems increasing the EPISLON alleviate the artifact
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
        return m_scene->castRay(ro, rd, EntityFilter::BakeInLightMap);
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
            {0.f, 0.f, 1.f},    // Up
            {0.f, 0.f, -1.f},   // Down
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
};