#include <thread>

#include "LightMap.h"
#include "Mesh.h"
#include "Texture.h"
#include "CyanAPI.h"
#include "RenderTarget.h"
#include "CyanRenderer.h"
#include "CyanPathTracer.h"
#include "stb_image_write.h"
#define MULTITHREAD_BAKE 0
namespace Cyan
{
    void LightMap::setPixel(u32 px, u32 py, glm::vec3& color)
    {
        u32 width = m_texAltas->m_width;
        u32 height = m_texAltas->m_height;
        const u32 numChannelsPerPixel = 3;

        u32 index = (py * width + px) * numChannelsPerPixel;
        CYAN_ASSERT(index < width * height * 3, "Writing to a pixel out of bound");
        m_pixels[index + 0] = color.r;
        m_pixels[index + 1] = color.g;
        m_pixels[index + 2] = color.b;
    }

    LightMapManager* LightMapManager::m_singleton = nullptr;
    LightMapManager::LightMapManager()
    {
        if (!m_singleton)
        {
            m_singleton = this;
            m_lightMapShader = createShader("LightMapShader", "../../shader/shader_lightmap.vs", "../../shader/shader_lightmap.fs");
            m_lightMapMatl = createMaterial(m_lightMapShader)->createInstance();

            // atomic counter
            glCreateBuffers(1, &texelCounterBuffer);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, texelCounterBuffer);
            glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
        }
        else
            cyanError("Creating multiple instances of LightMapManager");
    }
    
    LightMapManager* LightMapManager::getSingletonPtr()
    {
        return m_singleton;
    }

    void LightMapManager::renderMeshInstanceToLightMap(SceneNode* node, bool saveImage)
    {
        glDisable(GL_CULL_FACE);
        LightMap* lightMap = node->m_meshInstance->m_lightMap;
        Mesh* mesh = node->m_meshInstance->m_mesh;
        // raster the lightmap using gpu
        u32 maxNumTexels = lightMap->m_texAltas->m_height * lightMap->m_texAltas->m_width;
        {
            glm::mat4 model = node->getWorldTransform().toMatrix();
            auto ctx = getCurrentGfxCtx();
            ctx->setDepthControl(DepthControl::kDisable);
            ctx->setRenderTarget(lightMap->m_renderTarget, 0);
            ctx->setViewport({ 0, 0, lightMap->m_texAltas->m_width, lightMap->m_texAltas->m_height });
            ctx->setShader(m_lightMapShader);
            // reset the counter
            {
                u32 counter = 0;
                glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, texelCounterBuffer);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &counter);
            } 
            m_lightMapMatl->bindBuffer("LightMapData", lightMap->m_bakingGpuDataBuffer);
#if 1
            glEnable(GL_NV_conservative_raster);
            f32 uvOffset = 1.f / lightMap->m_texAltas->m_width; 
            glm::vec3 passes[5] = {
                glm::vec3(-1.f, 0.f, 0.f),
                glm::vec3( 1.f, 0.f, 0.f),
                glm::vec3( 0.f, 1.f, 0.f),
                glm::vec3( 0.f,-1.f, 0.f),
                glm::vec3( 0.f, 0.f, 0.f)
            };
            glm::vec3 textureSize(lightMap->m_texAltas->m_width, lightMap->m_texAltas->m_height, 0.f);
            m_lightMapMatl->set("textureSize", &textureSize.x);
            m_lightMapMatl->set("model", &model[0]);
            u32 numPasses = sizeof(passes) / sizeof(passes[0]);

            for (u32 pass = 0; pass < numPasses; ++pass)
            {
                glm::vec3 offset = passes[pass] * uvOffset;
                m_lightMapMatl->set("pass", (f32)pass);
                m_lightMapMatl->set("uvOffset", offset.x);
                m_lightMapMatl->bind();
                // reset the counter
                // {
                //     u32 counter = 0;
                //     glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, texelCounterBuffer);
                //     glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &counter);
                // } 
                for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
                {
                    auto ctx = getCurrentGfxCtx();
                    auto renderer = Renderer::getSingletonPtr();
                    ctx->setVertexArray(mesh->m_subMeshes[sm]->m_vertexArray);
                    ctx->drawIndex(mesh->m_subMeshes[sm]->m_numIndices);
                }
            }
            glDisable(GL_NV_conservative_raster);
#else
            glm::vec3 textureSize(lightMap->m_texAltas->m_width, lightMap->m_texAltas->m_height, 0.f);
            m_lightMapMatl->set("textureSize", &textureSize.x);
            m_lightMapMatl->set("model", &model[0]);
            m_lightMapMatl->bind();
            glEnable(GL_NV_conservative_raster);
            for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
            {
                auto ctx = getCurrentGfxCtx();
                auto renderer = Renderer::getSingletonPtr();
                ctx->setVertexArray(mesh->m_subMeshes[sm]->m_vertexArray);
                ctx->drawIndex(mesh->m_subMeshes[sm]->m_numIndices);
            }
            glDisable(GL_NV_conservative_raster);
#endif
            ctx->setDepthControl(DepthControl::kEnable);
        }
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        u32 overlappedTexelCount = 0;
        // read back lightmap texel data and run path tracing to bake each texel
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightMap->m_bakingGpuDataBuffer->m_ssbo);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(LightMapTexelData) * maxNumTexels, lightMap->m_bakingData.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, texelCounterBuffer);
            glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &overlappedTexelCount);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
            cyanInfo("%d overlapped pixels out of total %d, %.2f%% utilization", overlappedTexelCount, maxNumTexels, f32(overlappedTexelCount) * 100.f / maxNumTexels);
        }
        // baking lighting using path tracer
        {
            const f32 EPSILON = 0.001f;
            PathTracer* pathTracer = PathTracer::getSingletonPtr();
            pathTracer->setScene(lightMap->m_scene);
            u32 numBakedTexels = 0;
            // divide work into 8 threads
#if MULTITHREAD_BAKE
            std::thread worker00();
            std::thread worker01();
            std::thread worker02();
            std::thread worker03();
            worker00.join();
            worker01.join();
            worker02.join();
            worker03.join();
#endif
#if 0
            for (u32 i = 0; i < overlappedTexelCount; ++i)
            {
                glm::vec3 worldPos = vec4ToVec3(lightMap->m_bakingData[i].worldPosition);
                glm::vec3 normal = vec4ToVec3(lightMap->m_bakingData[i].normal);
                glm::vec2 texCoord = lightMap->m_bakingData[i].texCoord;
                glm::vec3 ro = worldPos + normal * EPSILON;
                glm::vec3 irradiance = pathTracer->sampleIrradiance(ro, normal);
                u32 px = floor(texCoord.x);
                u32 py = floor(texCoord.y);
                lightMap->setPixel(px, py, irradiance);
                printf("\r[Info] Baked %d texels ... %.2f%%", numBakedTexels, ((f32)numBakedTexels * 100.f / overlappedTexelCount));
                fflush(stdout);
                numBakedTexels++;
            }    
#endif
            for (u32 y = 0; y < lightMap->m_texAltas->m_height; ++y)
            {
                for (u32 x = 0; x < lightMap->m_texAltas->m_width; ++x)
                {
                    u32 i = lightMap->m_texAltas->m_width * y + x;
                    glm::vec3 worldPos = vec4ToVec3(lightMap->m_bakingData[i].worldPosition);
                    glm::vec3 normal = vec4ToVec3(lightMap->m_bakingData[i].normal);
                    glm::vec4 texCoord = lightMap->m_bakingData[i].texCoord;
                    if (texCoord.w == 1.f)
                    {
                        glm::vec3 ro = worldPos + normal * EPSILON;
                        glm::vec3 irradiance = pathTracer->sampleIrradiance(ro, normal);
                        u32 px = floor(texCoord.x);
                        u32 py = floor(texCoord.y);
                        lightMap->setPixel(px, py, irradiance);
                        printf("\r[Info] Baked %d texels ... %.2f%%", numBakedTexels, ((f32)numBakedTexels * 100.f / overlappedTexelCount));
                        fflush(stdout);
                        numBakedTexels++;
                    }
                }
            }
        }
        // write color data back to texture
        {
            u32 width = lightMap->m_texAltas->m_width;
            u32 height = lightMap->m_texAltas->m_height;
            glBindTexture(GL_TEXTURE_2D, lightMap->m_texAltas->m_id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, lightMap->m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (saveImage)
            stbi_write_hdr("room_lightmap.hdr", lightMap->m_texAltas->m_width, lightMap->m_texAltas->m_height, 3, lightMap->m_pixels);
        glEnable(GL_CULL_FACE);
    }

    void LightMapManager::createLightMapForMeshInstance(Scene* scene, SceneNode* node)
    {
        MeshInstance* meshInstance = node->m_meshInstance;         
        Mesh*         mesh =  meshInstance->m_mesh;
        meshInstance->m_lightMap = new LightMap;
        LightMap* lightMap = meshInstance->m_lightMap;
        {
            lightMap->m_owner = node;
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec  = { };
            spec.m_width    = meshInstance->m_mesh->m_lightMapWidth;
            spec.m_height   = meshInstance->m_mesh->m_lightMapHeight;
            spec.m_dataType = Texture::DataType::Float;
            spec.m_type     = Texture::Type::TEX_2D;
            spec.m_format   = Texture::ColorFormat::R16G16B16;
            spec.m_min      = Texture::Filter::LINEAR;
            spec.m_mag      = Texture::Filter::LINEAR;
            spec.m_numMips  = 1;

            lightMap->m_texAltas = textureManager->createTextureHDR("LightMap", spec);
            lightMap->m_renderTarget = createRenderTarget(meshInstance->m_lightMap->m_texAltas->m_width, meshInstance->m_lightMap->m_texAltas->m_height);
            lightMap->m_renderTarget->attachTexture(meshInstance->m_lightMap->m_texAltas, 0);
        }

        u32 maxNumTexels = lightMap->m_texAltas->m_height * lightMap->m_texAltas->m_width;
        lightMap->m_bakingGpuDataBuffer = createRegularBuffer(sizeof(LightMapTexelData) * maxNumTexels);
        lightMap->m_bakingData.resize(maxNumTexels);
        lightMap->m_pixels = new float[maxNumTexels * 3 * sizeof(float)];
        lightMap->m_scene = scene;
    }

    void LightMapManager::bakeLightMap(Scene* scene, SceneNode* node, bool saveImage)
    {
        createLightMapForMeshInstance(scene, node);
        renderMeshInstanceToLightMap(node, saveImage);
    }
}
