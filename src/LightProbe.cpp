#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "stb_image.h"

namespace Cyan
{
    Shader* s_renderProbeShader = 0;
    Shader* IrradianceProbe::m_computeIrradianceShader = 0;
    RenderTarget* IrradianceProbe::m_radianceRenderTarget = 0; 
    RenderTarget* IrradianceProbe::m_irradianceRenderTarget = 0; 
    Shader* LightFieldProbe::m_octProjectionShader = 0;
    RenderTarget* LightFieldProbe::m_cubemapRenderTarget = 0;
    RenderTarget* LightFieldProbe::m_octMapRenderTarget = 0;

    Shader* getRenderProbeShader()
    {
        if (!s_renderProbeShader)
        {
            s_renderProbeShader = createShader("RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs");
        }
        return s_renderProbeShader;
    }

    IrradianceProbe::IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(name , id, Transform(), parent), m_scene(scene)
    {
        if (!m_radianceRenderTarget)
        {
            m_radianceRenderTarget = createRenderTarget(512u, 512u);
            m_irradianceRenderTarget = createRenderTarget(64u, 64u);
            m_computeIrradianceShader = createShader("DiffuseIrradianceShader", "../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
            m_cubeMeshInstance = getMesh("CubeMesh")->createInstance();
            m_cubeMeshInstance->setMaterial(0, m_computeIrradianceMatl);
        }
        
        TextureSpec spec = { };
        spec.m_width = 512u;
        spec.m_height = 512u;
        spec.m_type = Texture::Type::TEX_CUBEMAP;
        spec.m_format = Texture::ColorFormat::R16G16B16;
        spec.m_dataType =  Texture::Float;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::NONE;
        spec.m_t = Texture::Wrap::NONE;
        spec.m_r = Texture::Wrap::NONE;
        spec.m_numMips = 1u;
        spec.m_data = 0;
        auto textureManager = TextureManager::getSingletonPtr();
        m_radianceMap = textureManager->createTextureHDR("RadianceProbe", spec);
        // FIXME: this is bugged, should only detach/attach when rendering
        m_radianceRenderTarget->attachColorBuffer(m_radianceMap);
        spec.m_width = 64u;
        spec.m_height = 64u;
        m_irradianceMap = textureManager->createTextureHDR("IrradianceProbe", spec);
        m_irradianceRenderTarget->attachColorBuffer(m_irradianceMap);

        m_computeIrradianceMatl = createMaterial(m_computeIrradianceShader)->createInstance();
        m_renderProbeMatl = createMaterial(getRenderProbeShader())->createInstance();

        Mesh* mesh = Cyan::getMesh("sphere_mesh");
        Transform transform;
        transform.m_translate = p;
        transform.m_scale = glm::vec3(0.2f);
        m_sceneRoot->attach(createSceneNode("SphereMesh", transform, mesh, false));
        setMaterial("SphereMesh", 0, m_renderProbeMatl);
        m_renderProbeMatl->bindTexture("radianceMap", m_irradianceMap);
        m_cubeMeshInstance->setMaterial(0, m_computeIrradianceMatl);

        auto renderMatlCallback = [&]()
        {
            m_renderProbeMatl->bindTexture("radianceMap", m_irradianceMap);
        };
    }

    void IrradianceProbe::sampleRadiance()
    {
        const u32 kViewportWidth = 512u;
        const u32 kViewportHeight = 512u;
        Camera camera = { };
        // camera set to probe's location
        camera.position = getSceneNode("SphereMesh")->getWorldTransform().m_translate;
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
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
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setViewport({0u, 0u, 512u, 512u});
        for (u32 f = 0; f < (sizeof(cameraTargets)/sizeof(cameraTargets[0])); ++f)
        {
            ctx->setRenderTarget(m_radianceRenderTarget, f);
            glClear(GL_DEPTH_BUFFER_BIT);
            camera.lookAt = cameraTargets[f];
            camera.view = glm::lookAt(camera.position, camera.position + cameraTargets[f], worldUps[f]);
            Renderer::getSingletonPtr()->renderScene(m_scene, camera);
        }
    }

    void IrradianceProbe::computeIrradiance()
    {
        Toolkit::ScopedTimer timer("ComputeIrradianceTimer");
        auto ctx = Cyan::getCurrentGfxCtx();
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
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

        auto renderer = Renderer::getSingletonPtr();
        ctx->setShader(m_computeIrradianceShader);
        ctx->setViewport({0u, 0u, m_irradianceRenderTarget->m_width, m_irradianceRenderTarget->m_height});
        ctx->setDepthControl(DepthControl::kDisable);
        for (u32 f = 0; f < 6u; ++f)
        {
            camera.lookAt = cameraTargets[f];
            camera.view = glm::lookAt(camera.position, camera.lookAt, worldUps[f]);
            ctx->setRenderTarget(m_irradianceRenderTarget, f);
            m_computeIrradianceMatl->set("view", &camera.view[0][0]);
            m_computeIrradianceMatl->set("projection", &camera.projection[0][0]);
            m_computeIrradianceMatl->bindTexture("envmapSampler", m_radianceMap);
            Renderer::getSingletonPtr()->drawMeshInstance(m_cubeMeshInstance, nullptr);
        }
        ctx->setDepthControl(DepthControl::kEnable);
        timer.end();
    }

    LightFieldProbe::LightFieldProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(name , id, Transform(), parent), m_scene(scene)
    {
        m_bakedInProbes = false;
        auto textureManager = TextureManager::getSingletonPtr();
        {
            TextureSpec spec = { };
            spec.m_width = 1024u;
            spec.m_height = 1024u;
            spec.m_type = Texture::Type::TEX_CUBEMAP;
            spec.m_format = Texture::ColorFormat::R16G16B16;
            spec.m_dataType = Texture::Float;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_numMips = 1u;
            spec.m_data = 0;
            m_radiance = textureManager->createTextureHDR("RadianceCubeMap", spec);
            m_normal = textureManager->createTextureHDR("NormalCubeMap", spec);
            spec.m_min = Texture::Filter::NEAREST;
            spec.m_mag = Texture::Filter::NEAREST;
            m_radialDistance = textureManager->createTextureHDR("RadialDistance", spec);

            TextureSpec octSpec = { };
            octSpec.m_width = 1024u;
            octSpec.m_height = 1024u;
            octSpec.m_type = Texture::Type::TEX_2D;
            octSpec.m_format = Texture::ColorFormat::R16G16B16;
            octSpec.m_dataType = Texture::Float;
            octSpec.m_min = Texture::Filter::NEAREST;
            octSpec.m_mag = Texture::Filter::NEAREST;
            octSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            octSpec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            octSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            octSpec.m_numMips = 1u;
            octSpec.m_data = 0;
            m_radianceOct = textureManager->createTextureHDR("RadianceOct", octSpec);
            m_normalOct = textureManager->createTextureHDR("NormalOct",   octSpec);
            m_distanceOct = textureManager->createTextureHDR("DistanceOct", octSpec);
        }
        if (!m_cubemapRenderTarget)
        {
            m_octProjectionShader = createShader("OctProjShader", "../../shader/shader_oct_mapping.vs", "../../shader/shader_oct_mapping.fs");
            m_cubemapRenderTarget = createRenderTarget(1024u, 1024u);
            m_octMapRenderTarget = createRenderTarget(1024u, 1024u);
        }
        Mesh* mesh = Cyan::getMesh("sphere_mesh");
        Transform transform;
        transform.m_translate = p;
        transform.m_scale = glm::vec3(0.2f);
        m_sceneRoot->attach(createSceneNode("SphereMesh", transform, mesh, false));
        m_renderProbeMatl = createMaterial(getRenderProbeShader())->createInstance();
        m_renderProbeMatl->bindTexture("radianceMap", m_radiance);
        m_octProjMatl = createMaterial(m_octProjectionShader)->createInstance();
        setMaterial("SphereMesh", 0, m_renderProbeMatl);

        m_octMapDebugLines[0] = new Line2D(glm::vec3(-1.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f));
        m_octMapDebugLines[1] = new Line2D(glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
        m_octMapDebugLines[2] = new Line2D(glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
        m_octMapDebugLines[3] = new Line2D(glm::vec3(0.f,  1.f, 0.f), glm::vec3(1.0f, 0.f, 0.f));
        m_octMapDebugLines[4] = new Line2D(glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
        m_octMapDebugLines[5] = new Line2D(glm::vec3(0.f, -1.f, 0.f), glm::vec3(-1.f, 0.f, 0.f));

        for (u32 line = 0; line < 6; ++line)
        {
            m_octMapDebugLines[line]->setColor(glm::vec4(1.f, 1.f, 0.f, 0.f));
        }
    }
    
    void LightFieldProbe::sampleScene()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // sample radiance into a cubemap
        {
            Camera camera = { };
            // camera set to probe's location
            camera.position = getSceneNode("SphereMesh")->getWorldTransform().m_translate;
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.001f, 100.f); 
            camera.n = 0.1f;
            camera.f = 100.f;
            camera.fov = glm::radians(90.f);
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
            ctx->setViewport({ 0u, 0u, m_cubemapRenderTarget->m_width, m_cubemapRenderTarget->m_height });

            // bundle entities that should be baked
            std::vector<Entity*> bakeEntities;
            bakeEntities.resize(m_scene->entities.size());
            u32 numBakedEntities = 0u;
            for (u32 i = 0; i < m_scene->entities.size(); ++i)
            {
                auto entity = m_scene->entities[i];
                if (entity->m_bakedInProbes)
                {
                    bakeEntities[numBakedEntities++] = entity;
                }
            }
            bakeEntities.resize(numBakedEntities);

            u32 drawBuffers[4] = { 0, 1, -1, 2 };
            m_cubemapRenderTarget->setDrawBuffers(drawBuffers, sizeof(drawBuffers) / sizeof(drawBuffers[0]));

            auto renderer = Renderer::getSingletonPtr();
            for (u32 f = 0; f < (sizeof(cameraTargets)/sizeof(cameraTargets[0])); ++f)
            {
                // rebind color attachments
                {
                    m_cubemapRenderTarget->attachTexture(m_radiance, 0, f);
                    m_cubemapRenderTarget->attachTexture(m_normal, 1, f);
                    m_cubemapRenderTarget->attachTexture(m_radialDistance, 2, f);
                }
                ctx->setRenderTarget(m_cubemapRenderTarget);
                ctx->clear();

                // update camera
                camera.lookAt = camera.position + cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);

                LightingEnvironment lighting = {
                    m_scene->pLights,
                    m_scene->dLights,
                    m_scene->m_currentProbe,
                    true
                };
                for (u32 i = 0; i < m_scene->dLights.size(); ++i)
                {
                    renderer->addDirectionalShadowPass(m_scene, camera, i);
                }
                renderer->addEntityPass(m_cubemapRenderTarget, {0, 0, m_cubemapRenderTarget->m_width, m_cubemapRenderTarget->m_height}, bakeEntities, lighting, camera);
                renderer->render();
            }
        }
        
        // resample radiance cubemap into a octmap using octahedral projection
        {
            ctx->setDepthControl(DepthControl::kDisable);
            u32 drawBuffers[3] = { 0, 1, 2 };
            m_octMapRenderTarget->attachTexture(m_radianceOct, 0u);
            m_octMapRenderTarget->attachTexture(m_normalOct, 1u);
            m_octMapRenderTarget->attachTexture(m_distanceOct, 2u);
            ctx->setRenderTarget(m_octMapRenderTarget, drawBuffers, 3);
            ctx->clear();
            ctx->setShader(m_octProjectionShader);
            m_octProjMatl->bindTexture("radiance", m_radiance);
            m_octProjMatl->bindTexture("normal", m_normal);
            m_octProjMatl->bindTexture("radialDepth", m_radialDistance);
            m_octProjMatl->bind();
            QuadMesh* quad = getQuadMesh();
            ctx->setVertexArray(quad->m_vertexArray);
            ctx->drawIndexAuto(quad->m_vertexArray->numVerts());
        }

        // draw debug lines
        {
            // ctx->setRenderTarget(m_octMapRenderTarget, 0u);
            // m_octMapDebugLines[0]->draw();
            // m_octMapDebugLines[1]->draw();
            // m_octMapDebugLines[2]->draw();
            // m_octMapDebugLines[3]->draw();
            // m_octMapDebugLines[4]->draw();
            // m_octMapDebugLines[5]->draw();
        }
        ctx->setDepthControl(DepthControl::kEnable);
    }

    void LightFieldProbe::save()
    {
        u32 bpp = 2u;
        u32 numPixels = m_radiance->m_width * m_radiance->m_height;
        void* pixels = _alloca(bpp * numPixels);
        glReadPixels(0, 0, m_radiance->m_width, m_radiance->m_height, GL_RGB, GL_FLOAT, pixels);
        // write to disk
    }

    LightFieldProbeVolume::LightFieldProbeVolume(glm::vec3& pos, glm::vec3& dimension, glm::vec3& spacing)
        : m_volumePos(pos), m_volumeDimension(dimension), m_probeSpacing(spacing)
    {
        glm::ivec3 numProbesDim = m_volumeDimension / spacing + glm::vec3(1.f);
        u32 numProbes = numProbesDim.x * numProbesDim.y * numProbesDim.z;
        m_probes.resize(numProbes);

        const u32 octMapWidth = 1024u; 
        const u32 octMapHeight = 1024u; 

        m_lowerLeftCorner = m_volumePos - .5f * glm::vec3(m_volumeDimension.x, m_volumeDimension.y, -m_volumeDimension.z);

        // create texture arrays
        {
            auto textureManager = TextureManager::getSingletonPtr();
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D_ARRAY;
            spec.m_dataType = Texture::DataType::Float;
            spec.m_width = 1024u;
            spec.m_height = 1024u;
            spec.m_depth = numProbes;
            spec.m_format = Texture::ColorFormat::R16G16B16;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            m_octRadianceGrid = textureManager->createArrayTexture2D("octRadianceGrid", spec);
            m_octRadialDepthGrid = textureManager->createArrayTexture2D("octRadialDepthGrid", spec);
            m_octNormalGrid = textureManager->createArrayTexture2D("octNormalGrid", spec);
        }
    }

    void LightFieldProbeVolume::sampleScene()
    {
        for (auto probe : m_probes)
        {
            probe->sampleScene();
        }
        
        packProbeTextures();
    }

    // pack probe textures into texture arrays
    void LightFieldProbeVolume::packProbeTextures()
    {
        // copy probe textures into the pre-allocated texture array
        // radiance
        {
            int numProbes = m_probes.size();
            for (int i = 0; i < numProbes; ++i)
            {
                int layer = i;
                // radiance
                {
                    glCopyImageSubData(
                        m_probes[i]->m_radianceOct->m_id, GL_TEXTURE_2D, 0, 0, 0, 0, 
                        m_octRadianceGrid->m_id, GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 
                        m_octRadianceGrid->m_width, m_octRadianceGrid->m_height, 1);
                }

                // normal
                {
                    glCopyImageSubData(
                        m_probes[i]->m_normalOct->m_id, GL_TEXTURE_2D, 0, 0, 0, 0, 
                        m_octNormalGrid->m_id, GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 
                        m_octNormalGrid->m_width, m_octNormalGrid->m_height, 1);
                }

                // radial depth
                {
                    glCopyImageSubData(
                        m_probes[i]->m_distanceOct->m_id, GL_TEXTURE_2D, 0, 0, 0, 0, 
                        m_octRadialDepthGrid->m_id, GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, 
                        m_octRadialDepthGrid->m_width, m_octRadialDepthGrid->m_height, 1);
                }
            }
        }
    }

    IrradianceProbe* LightProbeFactory::createIrradianceProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        auto probe = new IrradianceProbe("IrradianceProbe0", id, position, nullptr, scene);
        scene->entities.push_back(probe);
        return probe;
    }

    LightFieldProbe* LightProbeFactory::createLightFieldProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        auto probe = new LightFieldProbe("LightFieldProbe", id, position, nullptr, scene);
        scene->entities.push_back(probe);
        return probe;
    }

    LightFieldProbeVolume* LightProbeFactory::createLightFieldProbeVolume(Scene* scene, glm::vec3& position, glm::vec3& dimension, glm::vec3& spacing)
    {
        auto probeVolume = new LightFieldProbeVolume(position, dimension, spacing);
        glm::ivec3 numProbesDim = probeVolume->m_volumeDimension / spacing + glm::vec3(1.f);

        // start creating probes form lower left corner of the volume
        glm::vec3 lowerLeftPos = probeVolume->m_lowerLeftCorner;
        for (i32 k = 0; k < numProbesDim.y; ++k)
        {
            for (i32 j = 0; j < numProbesDim.z; ++j)
            {
                for (i32 i = 0; i < numProbesDim.x; ++i)
                {
                    glm::vec3 probePos = lowerLeftPos + glm::vec3(i, k, -j) * probeVolume->m_probeSpacing;
                    u32 probeIndex = k * (numProbesDim.x * numProbesDim.z) + j * (numProbesDim.x) + i;
                    probeVolume->m_probes[probeIndex] = createLightFieldProbe(scene, probePos);
                } 
            }
        }
        return probeVolume;
    }
}