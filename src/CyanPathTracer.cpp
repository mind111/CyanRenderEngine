#include <thread>
#include "CyanPathTracer.h"
#include "Texture.h"
#include "mathUtils.h"

#define EPISILON 0.0001f
namespace Cyan
{
    PathTracer::PathTracer(Scene* scene)
    : m_scene(scene)
    , m_pixels(nullptr)
    , m_texture(nullptr)
    , m_checkPoint(0u)
    , m_numTracedPixels(0u)
    , m_staticCamera(scene->getActiveCamera())
    , hitPositionCache{ }
    , numAccumulatedSamples(0)
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
    // TODO: diffuse shading
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

                glm::vec3 color = traceScene(ro, rd);
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

    void PathTracer::render(Scene* scene, Camera& camera)
    {
        // todo: split the work into 8 threads?
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        u32 boundryX = min(m_checkPoint.x + perFrameWorkGroupX, numPixelsInX);
        u32 boundryY = min(m_checkPoint.y + perFrameWorkGroupY, numPixelsInY);
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

                        pixelColor += traceScene(ro, rd);
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
        // CYAN_ASSERT((w + v) <= 1.f, "Invalid barycentric coordinates!\n");
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
        if (m_scene->castVisibilityRay(ro, rd))
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

    glm::vec3 getRayHitSurfaceAlbedo()
    {
        return glm::vec3(0.f);
    }

    // return surface normal at ray hit in world space
    glm::vec3 getRayHitSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord)
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

    glm::vec3 PathTracer::computeDirectLighting()
    {
        glm::vec3 color(0.f);
        return color;
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
            if (!m_scene->castVisibilityRay(ro, sampleDir))
            {
                outColor += skyColor * max(glm::dot(sampleDir, n), 0.f);
            }
        }
        return outColor / (f32)sampleCount;
    }

    glm::vec3 PathTracer::recursiveTraceDiffuse(glm::vec3& ro, glm::vec3& n, u32 numBounces)
    {
        if (numBounces >= 3)
            return glm::vec3(0.529f, 0.808f, 0.922f);
         
        glm::vec3 exitRadiance(0.f);
        glm::vec3 rd = uniformSampleHemiSphere(n);

        // ray cast
        auto nextRayHit = m_scene->castRay(ro, rd);

        if (nextRayHit.t > 0.f)
        {
            // compute geometric information
            glm::vec3 nextBounceRo = ro + nextRayHit.t * rd;
            glm::vec3 baryCoord = computeBaryCoordFromHit(nextRayHit, nextBounceRo);
            glm::vec3 nextBounceNormal = getRayHitSurfaceNormal(nextRayHit, baryCoord);

            // avoid self-intersecting
            nextBounceRo += EPISILON * nextBounceNormal;

            // indirect
            exitRadiance += 0.5f * recursiveTraceDiffuse(nextBounceRo, nextBounceNormal, numBounces + 1) * max(glm::dot(n, rd), 0.f);
        }

        // direct
        exitRadiance += computeDirectSkyLight(ro, n);
        exitRadiance *= glm::vec3(0.95f);
        return exitRadiance;
    }

    glm::vec3 PathTracer::traceScene(glm::vec3& ro, glm::vec3& rd)
    {
        auto hit = m_scene->castRay(ro, rd);
        glm::vec3 surfaceColor(0.0f);

        if (hit.t > 0.f)
        {
            auto worldTransformMatrix = hit.m_node->getWorldTransform().toMatrix();
            glm::vec3 hitPosition = ro + rd * hit.t;
            // convert world space hit back to object space
            glm::vec3 hitPosObjectSpace = vec4ToVec3(glm::inverse(worldTransformMatrix) * glm::vec4(hitPosition, 1.f));

            auto mesh = hit.m_node->m_meshInstance->m_mesh;
            auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex); 
            auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;
            u32 vertexOffset = hit.triIndex * 3;

            // material data
            auto meshInstance = hit.m_node->m_meshInstance;
            auto material = meshInstance->m_matls[hit.smIndex];
            auto albedoTexture = material->getTexture("diffuseMaps[0]");

            glm::vec3 albedo(1.f);

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoordFromHit(hit, hitPosition);
            glm::vec3 normal = getRayHitSurfaceNormal(hit, baryCoord);

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
            // for (auto dirLight : m_scene->dLights)
            // {
            //     glm::vec3 lightDir = dirLight.direction;
            //     // trace shadow ray
            //     f32 shadow = traceShadowRay(hitPosition + EPISILON * normal, lightDir);

            //     f32 ndotl = max(glm::dot(normal, lightDir), 0.f);
            //     glm::vec3 fr = (albedo / M_PI);
            //     glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
            //     surfaceColor += Li * fr * ndotl;
            // }

            // indirect lighting
            {
                glm::vec3 indirectRo = hitPosition + EPISILON * normal;
                surfaceColor += recursiveTraceDiffuse(indirectRo, normal, 2);
            }
        }
        return surfaceColor;
    }
};