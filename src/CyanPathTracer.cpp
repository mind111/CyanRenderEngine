#include <thread>

#include "CyanPathTracer.h"
#include "Texture.h"
#include "mathUtils.h"
#include "CyanAPI.h"

/* 
    * todo: (Feature) integrate intel's open image denoiser
    * todo: (Optimization) multi-threading
    * todo: (Optimization) do some profiling
    * todo: (Optimization) think about how can I further improve performance
    * todo: (Bug) debug invalid barycentric coord computed for some ray intersections.
    * todo: (Bug) debug weird "black" vertical line in the final render, increasing "EPSILON" seems alleviate the issue."
*/

// Is 2mm of an offset too large..?
#define EPSILON 0.002f
namespace Cyan
{
    PathTracer* PathTracer::m_singleton = nullptr;

    PathTracer::PathTracer()
    : m_scene(nullptr)
    , m_pixels(nullptr)
    , m_texture(nullptr)
    , m_checkPoint(0u)
    , m_numTracedPixels(0u)
    , numAccumulatedSamples(0)
    , m_skyColor(0.529f, 0.808f, 0.922f)
    , m_renderMode(RenderMode::BakeLightmap)
    {
        if (!m_singleton)
        {
            u32 bytesPerPixel = sizeof(float) * 3;
            u32 numPixels = numPixelsInX * numPixelsInY;
            m_pixels = (float*)new char[bytesPerPixel * numPixels];
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
    }

    glm::vec2 getScreenCoordOfPixel(u32 x, u32 y)
    {
        return glm::vec2(0.f);
    }

    void PathTracer::setPixel(u32 px, u32 py, glm::vec3& color)
    {
        u32 index = (py * numPixelsInX + px) * numChannelPerPixel;
        CYAN_ASSERT(index < numPixelsInX * numPixelsInY * numChannelPerPixel, "Writing to a pixel out of bound");
        m_pixels[index + 0] = color.r;
        m_pixels[index + 1] = color.g;
        m_pixels[index + 2] = color.b;
    }

    // TODO: multi-threading (opengl multi-threading)
    // TODO: glossy specular
    void PathTracer::progressiveRender(Scene* scene, Camera& camera)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        u32 boundryX = min(m_checkPoint.x + perFrameWorkGroupX, numPixelsInX);
        u32 boundryY = min(m_checkPoint.y + perFrameWorkGroupY, numPixelsInY);

        for (u32 x = m_checkPoint.x; x < boundryX; ++x)
        {
            for (u32 y = m_checkPoint.y; y < boundryY; ++y)
            {
                glm::vec2 pixelCenterCoord;
                pixelCenterCoord.x = ((f32)x / (f32)numPixelsInX) * 2.f - 1.f;
                pixelCenterCoord.y = ((f32)y / (f32)numPixelsInY) * 2.f - 1.f;
                pixelCenterCoord.x *= aspectRatio;
                // jitter the sub pixel samples

                glm::vec3 ro = camera.position;
                glm::vec3 rd = normalize(camera.forward * camera.n
                                       + camera.right   * pixelCenterCoord.x 
                                       + camera.up      * pixelCenterCoord.y);

                auto hit = traceScene(ro, rd);
                glm::vec3 color = renderSurface(hit, ro, rd);
                setPixel(x, numPixelsInY - y - 1, color);
                m_numTracedPixels += 1;
            }
            if (++m_checkPoint.x >= numPixelsInX)
            {
                m_checkPoint.x = 0u;
                m_checkPoint.y += perFrameWorkGroupY;
            }
        }

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void PathTracer::renderScene(Camera& camera)
    {
        // todo: split the work into 8 threads?
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        f32 pixelSize = 1.f / (f32)numPixelsInY;
        f32 subPixelSize = pixelSize / (f32)sppxCount;
        u32 subPixelSampleCount = sppxCount * sppyCount;

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
                        pixelColor += renderSurface(hit, ro, rd);
                    }
                }

                setPixel(x, numPixelsInY - y - 1, pixelColor / (f32)subPixelSampleCount);

                {
                    m_numTracedPixels += 1;
                    printf("\rPath tracing progress ...%.2f%", (f32)m_numTracedPixels * 100.f / totalNumRays);
                    fflush(stdout);
                }
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

    f32 PathTracer::traceShadowRay(glm::vec3& ro, glm::vec3& rd)
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

    void PathTracer::progressiveIndirectLighting()
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        u32 boundryX = min(m_checkPoint.x + perFrameWorkGroupX, numPixelsInX);
        u32 boundryY = min(m_checkPoint.y + perFrameWorkGroupY, numPixelsInY);

        for (u32 x = m_checkPoint.x; x < boundryX; ++x)
        {
            for (u32 y = m_checkPoint.y; y < boundryY; ++y)
            {
            }
        }

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
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
            f32 shadow = traceShadowRay(hitPosition + EPSILON * normal, lightDir);
            f32 ndotl = max(glm::dot(normal, lightDir), 0.f);
            glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
            color += Li * ndotl;
        }
        return color;
    }

    glm::vec3 gammaCorrect(glm::vec3& color, f32 gamma)
    {
        return glm::vec3(pow(color.r, gamma), pow(color.g, gamma), pow(color.b, gamma));
    }

    glm::vec3 PathTracer::computeDirectSkyLight(glm::vec3& ro, glm::vec3& n)
    {
        // TODO: sample skybox instead of using a constant sky color
        glm::vec3 skyColor(0.529f, 0.808f, 0.922f);
        glm::vec3 outColor(0.f);
        const u32 sampleCount = 1u;
        for (u32 i = 0u; i < sampleCount; ++i)
        {
            glm::vec3 sampleDir = uniformSampleHemiSphere(n);
            // the ray hit the skybox
            if (!m_scene->castVisibilityRay(ro, sampleDir, EntityFilter::BakeInLightMap))
            {
                outColor += skyColor * max(glm::dot(sampleDir, n), 0.f);
            }
        }
        return outColor / (f32)sampleCount;
    }

    glm::vec3 PathTracer::recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces)
    {
         
        glm::vec3 exitRadiance(0.f);

        // direct
        {
            exitRadiance += computeDirectSkyLight(ro, n);
            exitRadiance += computeDirectLighting(ro, n);
        }

        // stop recursion
        if (numBounces >= 2)
            return exitRadiance;

        glm::vec3 rd = uniformSampleHemiSphere(n);

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
            // todo: is there any better attenuation ..?
            f32 atten = .8f;
            exitRadiance += atten * recursiveTraceDiffuse(nextBounceRo, nextBounceNormal, numBounces + 1) * max(glm::dot(n, rd), 0.f);
        }

        return exitRadiance;
    }
    void PathTracer::run(Camera& camera)
    {
        switch (m_renderMode)
        {
            case RenderMode::BakeLightmap:
                bakeScene(camera);
                break;
            case RenderMode::Render:
                renderScene(camera);
                break;
            default:
                printf("Invalid PathTracer setting! \n");
                break;
        }
    }

    f32 saturate(f32 val)
    {
        return glm::clamp(val, 0.f, 1.f);
    }

    // reference: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    glm::vec3 ACESFilm(glm::vec3 x)
    {
        f32 a = 2.51f;
        f32 b = 0.03f;
        f32 c = 2.43f;
        f32 d = 0.59f;
        f32 e = 0.14f;
        glm::vec3 result = (x*(a*x+b))/(x*(c*x+d)+e);
        return glm::vec3(saturate(result.r), saturate(result.g), saturate(result.b)); 
    }

    void PathTracer::bakeScene(Camera& camera)
    {
        // todo: split the work into 8 threads?
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        f32 pixelSize = 1.f / (f32)numPixelsInY;
        f32 subPixelSize = pixelSize / (f32)sppxCount;
        u32 subPixelSampleCount = sppxCount * sppyCount;

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
                        pixelColor += bakeSurface(hit, ro, rd);
                    }
                }

                // tone mapping here
                glm::vec3 finalColor = pixelColor / (f32)subPixelSampleCount;
                {
                    finalColor = ACESFilm(finalColor);
                    finalColor = gammaCorrect(finalColor, 0.45f);
                }

                setPixel(x, numPixelsInY - y - 1, finalColor);

                {
                    m_numTracedPixels += 1;
                    printf("\r[Info]: Path tracing progress ...%.2f%", (f32)m_numTracedPixels * 100.f / totalNumRays);
                    fflush(stdout);
                }
            }
        }

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glm::vec3 PathTracer::renderSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd)
    {
        glm::vec3 surfaceColor(0.f);
        if (hit.t > 0.f)
        {
            glm::vec3 hitPosition = ro + rd * hit.t;

            auto mesh = hit.m_node->m_meshInstance->m_mesh;
            auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex); 
            auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;

            glm::vec3 albedo(1.f);

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
            glm::vec3 normal = getSurfaceNormal(hit, baryCoord);

    #if 0
            // texCoord
            glm::vec3 texCoord = baryCoord.x * triangles.m_texCoordArray[vertexOffset]
                                + baryCoord.y * triangles.m_texCoordArray[vertexOffset + 1]
                                + baryCoord.z * triangles.m_texCoordArray[vertexOffset + 2];
            texCoord.x = texCoord.x < 0.f ? 1.f + texCoord.x : texCoord.x;
            texCoord.y = texCoord.y < 0.f ? 1.f + texCoord.y : texCoord.y;

            if (albedoTexture->m_data)
                texelFetch(albedoTexture, texCoord); 
            else
                albedo = normal * .5f + glm::vec3(.5f);
            albedo = glm::vec3(texCoord.x, texCoord.y, 0.0f);
    #endif

            //direct lighting
            for (auto dirLight : m_scene->dLights)
            {
                glm::vec3 lightDir = dirLight.direction;
                // trace shadow ray
                f32 shadow = traceShadowRay(hitPosition + EPSILON * normal, lightDir);

                f32 ndotl = max(glm::dot(normal, lightDir), 0.f);
                glm::vec3 fr = (albedo / M_PI);
                glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
                surfaceColor += Li * fr * ndotl;
            }

            // indirect lighting
            {
                glm::vec3 indirectRo = hitPosition + EPSILON * normal;
                surfaceColor += recursiveTraceDiffuse(indirectRo, normal, 2);
            }
        }
        return surfaceColor;
    }

    glm::vec3 PathTracer::bakeSurface(RayCastInfo& hit, glm::vec3& ro, glm::vec3& rd)
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

            glm::vec3 indirectRo = hitPosition + EPSILON * normal;

            // bake static sky light
            radiance += computeDirectSkyLight(indirectRo, normal);

            // indirect lighting
            {
                radiance += recursiveTraceDiffuse(indirectRo, normal, 0);
            }
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

    // importace sampling the cosine lobe
    glm::vec3 PathTracer::sampleIrradiance(glm::vec3& samplePos, glm::vec3& n)
    {
        const u32 numSamples = 1;
        glm::vec3 irradiance(0.f);
        for (u32 i = 0; i < numSamples; ++i)
        {
            auto rd = uniformSampleHemiSphere(n);
            auto hit = traceScene(samplePos, rd);
            if (hit.t > 0.f)
            {
                // todo: instead of only consider diffuse reflections along the path, also include indirect specular
                auto radiance = bakeSurface(hit, samplePos, rd);
                irradiance += radiance * max(glm::dot(rd, n), 0.f) * (1.f / M_PI);
            }
            else
                irradiance += m_skyColor * max(glm::dot(rd, n), 0.f);
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