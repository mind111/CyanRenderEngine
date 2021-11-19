#include "CyanPathTracer.h"
#include "Texture.h"

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

    // TODO: change to SoA to see if it can improve performance
    void PathTracer::render(Scene* scene, Camera& camera)
    {
        f32 aspectRatio = (f32)numPixelsInX / numPixelsInY;
        u32 totalNumRays = numPixelsInX * numPixelsInY; 
        u32 boundryX = min(m_checkPoint.x + perFrameWorkGroupX, numPixelsInX);
        u32 boundryY = min(m_checkPoint.y + perFrameWorkGroupY, numPixelsInY);
#if 0
        for (u32 x = 0u; x < numPixelsInX; ++x)
        {
            for (u32 y = 0u; y < numPixelsInY; ++y)
            {
#endif
        for (u32 x = m_checkPoint.x; x < boundryX; ++x)
        {
            for (u32 y = m_checkPoint.y; y < boundryY; ++y)
            {
#if 1
                glm::vec2 pixelCenterCoord;
                pixelCenterCoord.x = ((f32)x / (f32)numPixelsInX) * 2.f - 1.f;
                pixelCenterCoord.y = ((f32)y / (f32)numPixelsInY) * 2.f - 1.f;
                pixelCenterCoord.x *= aspectRatio;
                // jitter the sub pixel samples

                glm::vec3 ro = camera.position;
                glm::vec3 rd = normalize(camera.forward * camera.n
                                        + camera.right  * pixelCenterCoord.x 
                                        + camera.up     * pixelCenterCoord.y);
                glm::vec3 color = traceScene(ro, rd);
                setPixel(x, y, color);
                m_numTracedRays += 1;
                printf("Path tracing progress ...%.2f%\n", (f32)m_numTracedRays * 100.f / totalNumRays);
#endif
            }
            if (++m_checkPoint.x >= numPixelsInX)
            {
                m_checkPoint.x = 0u;
                m_checkPoint.y += perFrameWorkGroupY;
            }
        }

        // copy data to gpu texture
        {
            // glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            // glTexSubImage2D(GL_TEXTURE_2D, 0, m_checkPoint.x, m_checkPoint.y, perFrameWorkGroupX, perFrameWorkGroupY, GL_RGB, GL_FLOAT, m_pixels);
            // glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_2D, m_texture->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numPixelsInX, numPixelsInY, GL_RGB, GL_FLOAT, m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& ro, glm::vec3& rd, RayCastInfo& hitInfo)
    {
        auto p = ro + rd * hitInfo.t;
        auto p0 = vec4ToVec3(tri.m_vertices[0].position);
        auto p1 = vec4ToVec3(tri.m_vertices[1].position);
        auto p2 = vec4ToVec3(tri.m_vertices[2].position);

        f32 totalArea = glm::length(glm::cross(p1 - p0, p2 - p0)) * .5f;
        f32 u = glm::cross(p1 - p0, p - p0).length() * 0.5f / totalArea;
        f32 v = glm::cross(p - p0, p1 - p0).length() * 0.5f / totalArea;
        f32 w = 1 - u - v;
        return glm::vec3(w, u, v);
    }

    glm::vec3 PathTracer::traceScene(glm::vec3& ro, glm::vec3& rd)
    {
        auto hit = m_scene->castRay(ro, rd);
        glm::vec3 surfaceColor(0.2f);

        if (hit.m_entity)
        {
            auto mesh = hit.m_node->getAttachedMesh()->m_mesh;
            auto& tri = mesh->m_subMeshes[hit.smIndex]->m_triangles[hit.triIndex];
            // material data
            glm::vec3 albedo = { 0.9f, 0.7f, 0.7f };

            // compute barycentric coord of the surface point that we hit 
            glm::vec3 baryCoord = computeBaryCoord(tri, ro, rd, hit);

            // use barycentric coord to interpolate vertex attributes
            glm::vec3 normal = baryCoord.x * tri.m_vertices[0].normal 
                             + baryCoord.y * tri.m_vertices[1].normal 
                             + baryCoord.z * tri.m_vertices[2].normal;
            normal = glm::normalize(normal);

            for (auto dirLight : m_scene->dLights)
            {
                glm::vec3 lightDir = dirLight.direction;
                f32 ndotl = glm::dot(normal, lightDir);
                glm::vec3 fr = (albedo / M_PI);
                surfaceColor += vec4ToVec3(dirLight.color) * dirLight.color.w * fr * ndotl;
            }
            for (auto pLight : m_scene->pLights)
            {
                continue;
            }
        }
        return surfaceColor;
    }
};