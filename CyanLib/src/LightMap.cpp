#include <thread>

#include "LightMap.h"
#include "Mesh.h"
#include "Texture.h"
#include "CyanAPI.h"
#include "RenderTarget.h"
#include "CyanRenderer.h"
#include "CyanPathTracer.h"
#include "stb_image_write.h"

#define MULTITHREAD_BAKE 1
#define SUPER_SAMPLING 1

namespace Cyan
{
#if 0
    std::atomic<u32> LightMapManager::progressCounter(0u);

    void LightMap::setPixel(u32 px, u32 py, glm::vec3& color)
    {
        u32 width = m_texAltas->width;
        u32 height = m_texAltas->height;
        const u32 numChannelsPerPixel = 3;

        u32 index = (py * width + px) * numChannelsPerPixel;
        CYAN_ASSERT(index < width * height * 3, "Writing to a pixel out of bound");
        m_pixels[index + 0] = color.r;
        m_pixels[index + 1] = color.g;
        m_pixels[index + 2] = color.b;
    }

    void LightMap::accumulatePixel(u32 px, u32 py, glm::vec3 & color)
    {
        u32 ppx = px / LightMapManager::kNumSubPixelSamples;
        u32 ppy = py / LightMapManager::kNumSubPixelSamples;
        u32 width = m_texAltas->width;
        u32 height = m_texAltas->height;
        const u32 numChannelsPerPixel = 3;
        u32 index = (ppy * width + ppx) * numChannelsPerPixel;
        m_pixels[index + 0] += color.r;
        m_pixels[index + 1] += color.g;
        m_pixels[index + 2] += color.b;
    }

    LightMapManager* LightMapManager::m_singleton = nullptr;
    LightMapManager::LightMapManager()
        : m_lightMapShader(nullptr), m_lightMapMatl(nullptr), texelCounterBuffer(-1)
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
    
    LightMapManager* LightMapManager::get()
    {
        return m_singleton;
    }
    
    void LightMapManager::createLightMapFromTexture(SceneNode* node, Texture* bakedTexture)
    {
        node->m_meshInstance->m_lightMap = new LightMap{ };
        node->m_meshInstance->m_lightMap->m_owner = node;
        node->m_meshInstance->m_lightMap->m_texAltas = bakedTexture;
    }

    void LightMapManager::renderMeshInstanceToLightMap(SceneNode* node, bool saveImage)
    {
        glDisable(GL_CULL_FACE);
        LightMap* lightMap = node->m_meshInstance->m_lightMap;
        Mesh* parent = node->m_meshInstance->m_mesh;
        // raster the lightmap using gpu
        u32 maxNumTexels = lightMap->m_texAltas->height * lightMap->m_texAltas->width;
        {
            glm::mat4 model = node->getWorldTransform().toMatrix();
            auto ctx = getCurrentGfxCtx();
            ctx->setDepthControl(DepthControl::kDisable);
            ctx->setRenderTarget(lightMap->m_renderTarget, { 0 });
            ctx->setViewport({ 0, 0, lightMap->m_texAltas->width, lightMap->m_texAltas->height });
            ctx->setShader(m_lightMapShader);
            // reset the counter
            {
                u32 counter = 0;
                glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, texelCounterBuffer);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &counter);
            } 
            m_lightMapMatl->bindBuffer("LightMapData", lightMap->m_bakingGpuDataBuffer);

            /* 
                Multi-tap rasterization to create a "ribbon" area around each chart to alleviate
                black seams when sampling lightmap, because bilinear filter texels close to edge of 
                a chart. Following ideas from https://ndotl.wordpress.com/2018/08/29/baking-artifact-free-lightmaps/
            */
            glm::vec3 uvOffset(1.f / lightMap->m_texAltas->width, 1.f / lightMap->m_texAltas->height, 0.f);
            glm::vec3 passes[9] = {
                glm::vec3(-1.f, 0.f, 0.f),
                glm::vec3( 1.f, 0.f, 0.f),
                glm::vec3( 0.f, 1.f, 0.f),
                glm::vec3( 0.f,-1.f, 0.f),

                glm::vec3(-1.f, -1.f, 0.f),
                glm::vec3( 1.f, 1.f, 0.f),
                glm::vec3(-1.f, 1.f, 0.f),
                glm::vec3( 1.f, -1.f, 0.f),

                glm::vec3( 0.f, 0.f, 0.f)
            };
            glm::vec3 textureSize(lightMap->m_texAltas->width, lightMap->m_texAltas->height, 0.f);
            m_lightMapMatl->set("textureSize", &textureSize.x);
            m_lightMapMatl->set("model", &model[0]);
            u32 numPasses = sizeof(passes) / sizeof(passes[0]);

            glEnable(GL_NV_conservative_raster);
            for (u32 pass = 0; pass < numPasses; ++pass)
            {
                glm::vec3 offset = passes[pass] * uvOffset;
                m_lightMapMatl->set("pass", (f32)pass);
                m_lightMapMatl->set("uvOffset", &offset.x);
                m_lightMapMatl->bindForDraw();
                for (u32 sm = 0; sm < parent->m_subMeshes.size(); ++sm)
                {
                    auto ctx = getCurrentGfxCtx();
                    auto renderer = Renderer::get();
                    ctx->setVertexArray(parent->m_subMeshes[sm]->m_vertexArray);
                    ctx->drawIndex(parent->m_subMeshes[sm]->m_numIndices);
                }
            }
            glDisable(GL_NV_conservative_raster);
            ctx->setDepthControl(DepthControl::kEnable);
            /* 
                Note: I haven't figure out why unbinding framebuffer here seems fixed the issue where
                the lightmap texture data is not updated by glTexSubImage2D
            */
            ctx->setRenderTarget(nullptr, { });
        }
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        u32 overlappedTexelCount = 0;
        // read back lightmap texel data and run path tracer to bake each texel
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightMap->m_bakingGpuDataBuffer->m_ssbo);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(LightMapTexelData) * maxNumTexels, lightMap->m_bakingData.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, texelCounterBuffer);
            glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &overlappedTexelCount);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
            cyanInfo("%d overlapped pixels out of total %d, %.2f%% utilization", overlappedTexelCount, maxNumTexels, f32(overlappedTexelCount) * 100.f / maxNumTexels);
        }

        // bake lighting using path tracer
#if MULTITHREAD_BAKE
        /* 
            This currently takes around 200 seconds to bake a 2k by 2k lightmap
            with 8 rays per lightmap texel
        */
        bakeMultiThread(lightMap, overlappedTexelCount);
#else
        bakeSingleThread(lightMap, overlappedTexelCount);
#endif
        // write color data back to texture
        {
            u32 width = lightMap->m_texAltas->width;
            u32 height = lightMap->m_texAltas->height;
            glBindTexture(GL_TEXTURE_2D, lightMap->m_texAltas->handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, lightMap->m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        // write the texture as an image to disk
        if (saveImage)
        {
            char fileName[64];
            sprintf_s(fileName, "%s_lightmap.hdr", node->m_name);
            stbi_flip_vertically_on_write(1);
            stbi_write_hdr(fileName, lightMap->m_texAltas->width, lightMap->m_texAltas->height, 3, lightMap->m_pixels);
        }
        glEnable(GL_CULL_FACE);
    }

    void LightMapManager::renderMeshInstanceToSuperSampledLightMap(SceneNode* node, bool saveImage)
    {
        glDisable(GL_CULL_FACE);
        LightMap* lightMap = node->m_meshInstance->m_lightMap;
        CYAN_ASSERT(lightMap->superSampledTex, "SuperSampled texture is not created for lightMap");
        Mesh* parent = node->m_meshInstance->m_mesh;
        // raster the lightmap using gpu
        u32 maxNumTexels = lightMap->superSampledTex->width * lightMap->superSampledTex->height;
        {
            glm::mat4 model = node->getWorldTransform().toMatrix();
            auto ctx = getCurrentGfxCtx();
            ctx->setDepthControl(DepthControl::kDisable);
            ctx->setRenderTarget(lightMap->m_renderTarget, { 0 });
            ctx->setViewport({ 0, 0, lightMap->superSampledTex->width, lightMap->superSampledTex->height });
            ctx->setShader(m_lightMapShader);
            // reset the counter
            {
                u32 counter = 0;
                glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, texelCounterBuffer);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &counter);
            } 
            m_lightMapMatl->bindBuffer("LightMapData", lightMap->m_bakingGpuDataBuffer);

            /* 
                Multi-tap rasterization to create a "ribbon" area around each chart to alleviate
                black seams when sampling lightmap, because bilinear filter texels close to edge of 
                a chart. Following ideas from https://ndotl.wordpress.com/2018/08/29/baking-artifact-free-lightmaps/
            */
            glm::vec3 uvOffset(1.f / lightMap->superSampledTex->width, 1.f / lightMap->superSampledTex->height, 0.f);
            glm::vec3 passes[9] = {
                glm::vec3(-1.f, 0.f, 0.f),
                glm::vec3( 1.f, 0.f, 0.f),
                glm::vec3( 0.f, 1.f, 0.f),
                glm::vec3( 0.f,-1.f, 0.f),

                glm::vec3(-1.f, -1.f, 0.f),
                glm::vec3( 1.f, 1.f, 0.f),
                glm::vec3(-1.f, 1.f, 0.f),
                glm::vec3( 1.f, -1.f, 0.f),

                glm::vec3( 0.f, 0.f, 0.f)
            };
            glm::vec3 textureSize(lightMap->superSampledTex->width, lightMap->superSampledTex->height, 0.f);
            m_lightMapMatl->set("textureSize", &textureSize.x);
            m_lightMapMatl->set("model", &model[0]);
            u32 numPasses = sizeof(passes) / sizeof(passes[0]);

            glEnable(GL_NV_conservative_raster);
            for (u32 pass = 0; pass < numPasses; ++pass)
            {
                glm::vec3 offset = passes[pass] * uvOffset * (f32)kNumSubPixelSamples;
                m_lightMapMatl->set("pass", (f32)pass);
                m_lightMapMatl->set("uvOffset", &offset.x);
                m_lightMapMatl->bindForDraw();
                for (u32 sm = 0; sm < parent->m_subMeshes.size(); ++sm)
                {
                    auto ctx = getCurrentGfxCtx();
                    auto renderer = Renderer::get();
                    ctx->setVertexArray(parent->m_subMeshes[sm]->m_vertexArray);
                    ctx->drawIndex(parent->m_subMeshes[sm]->m_numIndices);
                }
            }
            glDisable(GL_NV_conservative_raster);
            ctx->setDepthControl(DepthControl::kEnable);
            /* 
                Note: I haven't figure out why unbinding framebuffer here seems fixed the issue where
                the lightmap texture data is not updated by glTexSubImage2D
            */
            ctx->setRenderTarget(nullptr, { });
        }
#if 1
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        u32 overlappedTexelCount = 0;
        // read back lightmap texel data and run path tracer to bake each texel
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightMap->m_bakingGpuDataBuffer->m_ssbo);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(LightMapTexelData) * maxNumTexels, lightMap->m_bakingData.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, texelCounterBuffer);
            glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(u32), &overlappedTexelCount);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
            cyanInfo("%d overlapped pixels out of total %d, %.2f%% utilization", overlappedTexelCount, maxNumTexels, f32(overlappedTexelCount) * 100.f / maxNumTexels);
        }

        // bake lighting using path tracer
#if MULTITHREAD_BAKE
        /* 
            This currently takes around 200 seconds to bake a 2k by 2k lightmap
            with 8 rays per lightmap texel
        */
        bakeMultiThread(lightMap, overlappedTexelCount);
#else
        bakeSingleThread(lightMap, overlappedTexelCount);
#endif
        // resolve
        {
            for (u32 y = 0; y < lightMap->m_texAltas->height; ++y)
            {
                for (u32 x = 0; x < lightMap->m_texAltas->width; ++x)
                {
                    u32 pixelIndex = (y * lightMap->m_texAltas->width + x);
                    lightMap->m_pixels[pixelIndex * 3 + 0] /= (kNumSubPixelSamples * kNumSubPixelSamples);
                    lightMap->m_pixels[pixelIndex * 3 + 1] /= (kNumSubPixelSamples * kNumSubPixelSamples);
                    lightMap->m_pixels[pixelIndex * 3 + 2] /= (kNumSubPixelSamples * kNumSubPixelSamples);
                }
            }
        }
        // write color data back to texture
        {
            u32 width = lightMap->m_texAltas->width;
            u32 height = lightMap->m_texAltas->height;
            glBindTexture(GL_TEXTURE_2D, lightMap->m_texAltas->handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, lightMap->m_pixels);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        // write the texture as an image to disk
        if (saveImage)
        {
            char fileName[64];
            sprintf_s(fileName, "%s_lightmap.hdr", node->m_name);
            stbi_flip_vertically_on_write(1);
            stbi_write_hdr(fileName, lightMap->m_texAltas->width, lightMap->m_texAltas->height, 3, lightMap->m_pixels);
        }
#endif
        glEnable(GL_CULL_FACE);
    }

    void LightMapManager::bakeSingleThread(LightMap* lightMap, u32 overlappedTexelCount)
    {
        const f32 EPSILON = 0.001f;
        PathTracer* pathTracer = PathTracer::get();
        u32 numBakedTexels = 0;
        for (u32 y = 0; y < lightMap->m_texAltas->height; ++y)
        {
            for (u32 x = 0; x < lightMap->m_texAltas->width; ++x)
            {
                u32 i = lightMap->m_texAltas->width * y + x;
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

    void LightMapManager::bakeWorker(LightMap* lightMap, std::vector<u32>& texelIndices, u32 start, u32 end, u32 overlappedTexelCount)
    {
        const f32 EPSILON = 0.0001f;
        PathTracer* pathTracer = PathTracer::get();
        for (u32 i = start; i < end; ++i)
        {
            u32 index = texelIndices[i];
            glm::vec3 worldPos = vec4ToVec3(lightMap->m_bakingData[index].worldPosition);
            glm::vec3 normal = vec4ToVec3(lightMap->m_bakingData[index].normal);
            glm::vec4 texCoord = lightMap->m_bakingData[index].texCoord;
            glm::vec3 ro = worldPos + normal * EPSILON;
            glm::vec3 irradiance = pathTracer->importanceSampleIrradiance(ro, normal);
            progressCounter.fetch_add(1);
            u32 px = floor(texCoord.x);
            u32 py = floor(texCoord.y);
#if SUPER_SAMPLING
            lightMap->accumulatePixel(px, py, irradiance);
#else
            lightMap->setPixel(px, py, irradiance);
#endif
        }
    }

    void LightMapManager::bakeMultiThread(LightMap* lightMap, u32 overlappedTexelCount)
    {
        Toolkit::ScopedTimer bakeTimer("bakeMultiThread", true);
        std::vector<u32> texelIndices(overlappedTexelCount);
        u32 numTexels = 0;
#if SUPER_SAMPLING
        u32 textureWidth = lightMap->superSampledTex->width;
        u32 textureHeight = lightMap->superSampledTex->height;
#else
        u32 textureWidth = lightMap->m_texAltas->m_width;
        u32 textureHeight = lightMap->m_texAltas->m_height;
#endif
        for (u32 y = 0; y < textureHeight; ++y)
        {
            for (u32 x = 0; x < textureWidth; ++x)
            {
                u32 i = textureWidth * y + x;
                glm::vec4 texCoord = lightMap->m_bakingData[i].texCoord;
                if (texCoord.w == 1.f)
                    texelIndices[numTexels++] = i;
            }
        }
        const u32 workGroupCount = 16;
        u32 workGroupPixelCount = overlappedTexelCount / workGroupCount;
        // divide work into 16 threads
        std::thread* workers[workGroupCount] = { };
        for (u32 i = 0; i < workGroupCount; ++i)
        {
            u32 start = workGroupPixelCount * i;
            u32 end = min(start + workGroupPixelCount, overlappedTexelCount);
            workers[i] = new std::thread(&LightMapManager::bakeWorker, this, lightMap, std::ref(texelIndices), start, end, overlappedTexelCount);
        }
        // log progress
        while (progressCounter.load() < overlappedTexelCount)
        {
            printf("\r[Info] Baked %d/%d texels ... %.2f%%", progressCounter.load(), overlappedTexelCount, ((f32)progressCounter.load() * 100.f / overlappedTexelCount));
            fflush(stdout);
        }

        for (u32 i = 0; i < workGroupCount; ++i)
            workers[i]->join();
        printf("\n");

        // free resources
        progressCounter = 0;
        for (u32 i = 0; i < workGroupCount; ++i)
            delete workers[i];
    }

    void LightMapManager::createLightMapForMeshInstance(Scene* scene, SceneNode* node)
    {
        MeshInstance* meshInstance = node->m_meshInstance;         
        Mesh*         parent =  meshInstance->m_mesh;
        CYAN_ASSERT(!meshInstance->m_lightMap, "lightMap is not properly initialized to nullptr");
        meshInstance->m_lightMap = new LightMap{ };
        meshInstance->m_lightMap->m_owner = node;
        LightMap* lightMap = meshInstance->m_lightMap;
        {
            lightMap->m_owner = node;
            auto textureManager = TextureManager::get();
            TextureSpec spec  = { };
            spec.width    = meshInstance->m_mesh->m_lightMapWidth;
            spec.height   = meshInstance->m_mesh->m_lightMapHeight;
            spec.dataType = Texture::DataType::Float;
            spec.type     = Texture::Type::TEX_2D;
            spec.format   = Texture::ColorFormat::R16G16B16;
            spec.min      = Texture::Filter::LINEAR;
            spec.mag      = Texture::Filter::LINEAR;
            spec.r        = Texture::Wrap::CLAMP_TO_EDGE;
            spec.s        = Texture::Wrap::CLAMP_TO_EDGE;
            spec.t        = Texture::Wrap::CLAMP_TO_EDGE;
            spec.numMips  = 1;

            lightMap->m_texAltas = textureManager->createTextureHDR("LightMap", spec);
#if SUPER_SAMPLING
            spec.width    = meshInstance->m_mesh->m_lightMapWidth * kNumSubPixelSamples;
            spec.height   = meshInstance->m_mesh->m_lightMapHeight * kNumSubPixelSamples;
            lightMap->superSampledTex = textureManager->createTextureHDR("SuperSampledLightMap", spec);
            lightMap->m_renderTarget = createRenderTarget(meshInstance->m_lightMap->superSampledTex->width, meshInstance->m_lightMap->superSampledTex->height);
            lightMap->m_renderTarget->setColorBuffer(meshInstance->m_lightMap->superSampledTex, 0u);
#else
            lightMap->m_renderTarget = createRenderTarget(meshInstance->m_lightMap->m_texAltas->m_width, meshInstance->m_lightMap->m_texAltas->m_height);
            lightMap->m_renderTarget->attachTexture(meshInstance->m_lightMap->m_texAltas, 0);
#endif
        }

        u32 baseNumTexels = lightMap->m_texAltas->width * lightMap->m_texAltas->height;
#if SUPER_SAMPLING
        u32 maxNumTexels = lightMap->superSampledTex->height * lightMap->superSampledTex->width;
#else
        u32 maxNumTexels = lightMap->m_texAltas->m_height * lightMap->m_texAltas->m_width;
#endif
        lightMap->m_bakingGpuDataBuffer = createRegularBuffer(sizeof(LightMapTexelData) * maxNumTexels);
        lightMap->m_bakingData.resize(maxNumTexels);
        lightMap->m_pixels = new f32[(u64)baseNumTexels * 3];
        // clear to 0
        memset(lightMap->m_pixels, 0x0, sizeof(f32) * 3 * baseNumTexels);
        lightMap->m_scene = scene;
    }

    void LightMapManager::bakeLightMap(Scene* scene, SceneNode* node, bool saveImage)
    {
        PathTracer* pathTracer = PathTracer::get();
        if (scene != pathTracer->m_scene)
            pathTracer->setScene(scene);
        createLightMapForMeshInstance(scene, node);
#if SUPER_SAMPLING
        renderMeshInstanceToSuperSampledLightMap(node, saveImage);
#else
        renderMeshInstanceToLightMap(node, saveImage);
#endif

    }
#endif
}
