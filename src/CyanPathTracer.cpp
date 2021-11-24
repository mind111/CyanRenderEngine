#include <thread>
#include "CyanPathTracer.h"
#include "Texture.h"

#define EPISILON 0.00001f
namespace Cyan
{
    PathTracer::PathTracer(Scene* scene)
    : m_scene(scene)
    , m_pixels(nullptr)
    , m_texture(nullptr)
    , m_checkPoint(0u)
    , m_numTracedRays(0u)
    , m_staticCamera(scene->getActiveCamera())
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

    // TODO: multi-threading
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
                m_numTracedRays += 1;
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

    // TODO: fix the camera to match raster pipeline
    void PathTracer::render(Scene* scene, Camera& camera)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        u32 boundryX = min(m_checkPoint.x + perFrameWorkGroupX, numPixelsInX);
        u32 boundryY = min(m_checkPoint.y + perFrameWorkGroupY, numPixelsInY);
        f32 w = camera.n * glm::tan(glm::radians(.5f * 45.f));

        for (u32 x = 0u; x < numPixelsInX; ++x)
        {
            for (u32 y = 0u; y < numPixelsInY; ++y)
            {
                glm::vec2 pixelCenterCoord;
                pixelCenterCoord.x = ((f32)x / (f32)numPixelsInX) * 2.f - 1.f;
                pixelCenterCoord.y = ((f32)y / (f32)numPixelsInY) * 2.f - 1.f;
                pixelCenterCoord.x *= aspectRatio;
                // jitter the sub pixel samples

                glm::vec3 ro = camera.position;
                glm::vec3 rd = normalize(camera.forward * camera.n
                                       + camera.right   * pixelCenterCoord.x * w 
                                       + camera.up      * pixelCenterCoord.y * w);

                glm::vec3 color = traceScene(ro, rd);
                setPixel(x, numPixelsInY - y - 1, color);
                m_numTracedRays += 1;
                printf("Path tracing progress ...%.2f%\n", (f32)m_numTracedRays * 100.f / totalNumRays);
            }
        }

        // copy data to gpu texture
        {
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& ro, glm::vec3& rd, glm::vec3& hit)
    {
        auto p = hit;
        auto p0 = tri.m_vertices[0];
        auto p1 = tri.m_vertices[1];
        auto p2 = tri.m_vertices[2];

        f32 totalArea = glm::length(glm::cross(p1 - p0, p2 - p0));

        f32 w = glm::length(glm::cross(p1 - p0, p - p0)) / totalArea;
        f32 v = glm::length(glm::cross(p2 - p0, p - p0)) / totalArea;
        CYAN_ASSERT((w + v) <= 1.f, "Invalid barycentric coordinates!\n");
        f32 u = 1.f - v - w;
        return glm::vec3(u, v, w);
    }

    f32 PathTracer::traceShadowRay(glm::vec3& ro, glm::vec3& rd)
    {
        f32 shadow = 1.f;
        if (m_scene->castVisibilityRay(ro, rd))
            shadow = 0.f;
        return shadow;
    }

    glm::vec3 vertexTransformWorldToObject(glm::vec3& position, glm::mat4& worldTransformMat)
    {
        return vec4ToVec3(glm::inverse(worldTransformMat) * glm::vec4(position, 1.f));
    }

    glm::vec3 PathTracer::traceScene(glm::vec3& ro, glm::vec3& rd)
    {
        auto hit = m_scene->castRay(ro, rd);
        glm::vec3 surfaceColor(0.0f);

        if (hit.m_entity)
        {
            auto worldTransformMatrix = hit.m_node->getWorldTransform().toMatrix();
            glm::vec3 hitPosition = ro + rd * hit.t;
            // convert world space hit back to object space
            glm::vec3 hitPosObjectSpace = vec4ToVec3(glm::inverse(worldTransformMatrix) * glm::vec4(hitPosition, 1.f));

            auto mesh = hit.m_node->m_meshInstance->m_mesh;
            // auto tri = mesh->getTriangle(hit.smIndex, hit.triIndex); 
            auto& triangles = mesh->m_subMeshes[hit.smIndex]->m_triangles;
            u32 vertexOffset = hit.triIndex * 3;
            // object space triangle
            Triangle tri = {
                triangles.m_positionArray[vertexOffset],
                triangles.m_positionArray[vertexOffset + 1],
                triangles.m_positionArray[vertexOffset + 2]
            };

            // material data
            glm::vec3 albedo = { 0.f, 0.f, 0.f };

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoord(tri, ro, rd, hitPosObjectSpace);

            // use barycentric coord to interpolate vertex attributes
            glm::vec3 normal = baryCoord.x * triangles.m_normalArray[vertexOffset]
                             + baryCoord.y * triangles.m_normalArray[vertexOffset + 1]
                             + baryCoord.z * triangles.m_normalArray[vertexOffset + 2];
            // convert normal to world space
            normal = vec4ToVec3(glm::inverse(glm::transpose(worldTransformMatrix)) * glm::vec4(normal, 0.f));
            normal = glm::normalize(normal);

            albedo = normal * .5f + glm::vec3(.5f);

            for (auto dirLight : m_scene->dLights)
            {
                glm::vec3 lightDir = dirLight.direction;
                // trace shadow ray
                f32 shadow = traceShadowRay(hitPosition + EPISILON * normal, lightDir);

                f32 ndotl = max(glm::dot(normal, lightDir), 0.f);
                glm::vec3 fr = (albedo / M_PI);
                glm::vec3 Li = vec4ToVec3(dirLight.color) * dirLight.color.w * shadow;
                surfaceColor += Li * fr * ndotl;
            }
        }
        return surfaceColor;
    }
};