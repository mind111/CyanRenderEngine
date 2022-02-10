#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "stb_image.h"

namespace Cyan
{
    Shader*        s_renderProbeShader                        = nullptr;
    Shader*        IrradianceProbe::m_computeIrradianceShader = nullptr;
    RenderTarget*  IrradianceProbe::m_radianceRenderTarget    = nullptr; 
    RenderTarget*  IrradianceProbe::m_irradianceRenderTarget  = nullptr; 
    MeshInstance*  IrradianceProbe::m_cubeMeshInstance        = nullptr; 
    RegularBuffer* IrradianceProbe::m_rayBuffers              = nullptr;
    RenderTarget*  ReflectionProbe::m_renderTarget            = nullptr;
    RenderTarget*  ReflectionProbe::m_prefilterRts[kNumMips]  = {  };
    Shader*        ReflectionProbe::m_prefilterShader         = nullptr;

    Shader* getRenderProbeShader()
    {
        if (!s_renderProbeShader)
        {
            s_renderProbeShader = createShader("RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs");
        }
        return s_renderProbeShader;
    }

    IrradianceProbe::IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(scene, name , id, Transform(), parent, false), 
        m_scene(scene)
    {
        if (!m_radianceRenderTarget)
        {
            m_radianceRenderTarget = createRenderTarget(512u, 512u);
            m_irradianceRenderTarget = createRenderTarget(64u, 64u);
            m_computeIrradianceShader = createShader("DiffuseIrradianceShader", "../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
            m_cubeMeshInstance = getMesh("CubeMesh")->createInstance(scene);
            m_cubeMeshInstance->setMaterial(0, m_computeIrradianceMatl);
        }
        
        m_visible = false;
        TextureSpec spec = { };
        spec.m_width = 512u;
        spec.m_height = 512u;
        spec.m_type = Texture::Type::TEX_CUBEMAP;
        spec.m_format = Texture::ColorFormat::R16G16B16;
        spec.m_dataType =  Texture::Float;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_numMips = 1u;
        spec.m_data = 0;
        auto textureManager = TextureManager::getSingletonPtr();
        auto sceneManager = SceneManager::getSingletonPtr();
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
        m_sceneRoot->attach(sceneManager->createSceneNode(scene, "SphereMesh", transform, mesh, false));
        setMaterial("SphereMesh", 0, m_renderProbeMatl);
        m_renderProbeMatl->bindTexture("radianceMap", m_irradianceMap);
        m_cubeMeshInstance->setMaterial(0, m_computeIrradianceMatl);
    }

    // TODO: exclude dynamic objects
    void IrradianceProbe::sampleRadiance()
    {
        const u32 kViewportWidth = 512u;
        const u32 kViewportHeight = 512u;
        Camera camera = { };
        // camera set to probe's location
        camera.position = getSceneNode("SphereMesh")->getWorldTransform().m_translate;
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
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

        auto renderer = Renderer::getSingletonPtr();
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setViewport({0u, 0u, 512u, 512u});
        for (u32 f = 0; f < (sizeof(cameraTargets)/sizeof(cameraTargets[0])); ++f)
        {
            i32 drawBuffers[4] = {(i32)f, -1, -1, -1};
            m_radianceRenderTarget->setDrawBuffers(drawBuffers, 4);
            ctx->setRenderTarget(m_radianceRenderTarget);
            ctx->clear();

            camera.lookAt = camera.position + cameraTargets[f];
            camera.worldUp = worldUps[f];
            CameraManager::updateCamera(camera);
            renderer->probeRenderScene(m_scene, camera);
        }
    }

    void IrradianceProbe::computeIrradiance()
    {
        Toolkit::GpuTimer timer("ComputeIrradianceTimer");
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
        SceneNode* node = getSceneNode("SphereMesh");
        for (u32 f = 0; f < 6u; ++f)
        {
            camera.lookAt = cameraTargets[f];
            camera.view = glm::lookAt(camera.position, camera.lookAt, worldUps[f]);
            ctx->setRenderTarget(m_irradianceRenderTarget, f);
            m_computeIrradianceMatl->set("view", &camera.view[0][0]);
            m_computeIrradianceMatl->set("projection", &camera.projection[0][0]);
            m_computeIrradianceMatl->bindTexture("envmapSampler", m_radianceMap);
            Renderer::getSingletonPtr()->drawMeshInstance(m_cubeMeshInstance, node->globalTransform);
        }
        ctx->setDepthControl(DepthControl::kEnable);
        timer.end();
    }

    ReflectionProbe::ReflectionProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(scene, name , id, Transform(), parent, false), m_scene(scene)
    {
        if (!m_renderTarget)
        {
            m_renderTarget = createRenderTarget(2048u, 2048u);
            m_prefilterShader = createShader("PrefilterSpecularShader", "../../shader/shader_prefilter_specular.vs", "../../shader/shader_prefilter_specular.fs");
            u32 mipWidth = 2048;
            u32 mipHeight = 2048;
            for (u32 mip = 0; mip < kNumMips; ++mip)
            {
                m_prefilterRts[mip] = createRenderTarget(mipWidth, mipHeight);
                mipWidth  /= 2;
                mipHeight /= 2;
            }
        }
        m_visible = false;
        TextureSpec spec = { };
        spec.m_width = 2048u;
        spec.m_height = 2048u;
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
        auto textureManager = TextureManager::getSingletonPtr();
        auto sceneManager = SceneManager::getSingletonPtr();
        m_radianceMap = textureManager->createTextureHDR("RadianceProbe", spec);
        // FIXME: this is bugged, should only detach/attach when rendering
        m_renderTarget->attachColorBuffer(m_radianceMap, 0);
        m_renderProbeMatl = createMaterial(getRenderProbeShader())->createInstance();

        Mesh* mesh = Cyan::getMesh("sphere_mesh");
        Transform transform;
        transform.m_translate = p;
        transform.m_scale = glm::vec3(1.0f);
        m_sceneRoot->attach(sceneManager->createSceneNode(scene, "SphereMesh", transform, mesh, false));
        setMaterial("SphereMesh", 0, m_renderProbeMatl);

        spec.m_numMips = 11u;
        spec.m_min = Texture::Filter::MIPMAP_LINEAR;
        m_prefilteredProbe = textureManager->createTextureHDR("PrefilteredReflectionProbe", spec);
        m_prefilterMatl = createMaterial(m_prefilterShader)->createInstance();
        m_renderProbeMatl->bindTexture("radianceMap", m_prefilteredProbe);
    }

    void ReflectionProbe::bake()
    {
        sampleRadiance();
        prefilter();
    }
    
    void ReflectionProbe::sampleRadiance()
    {
        const u32 kViewportWidth = m_radianceMap->m_width;
        const u32 kViewportHeight = m_radianceMap->m_height;
        Camera camera = { };
        // camera set to probe's location
        camera.position = getSceneNode("SphereMesh")->getWorldTransform().m_translate;
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
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
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setViewport({0u, 0u, kViewportWidth, kViewportHeight});
        for (u32 f = 0; f < (sizeof(cameraTargets)/sizeof(cameraTargets[0])); ++f)
        {
            i32 drawBuffers[4] = {(i32)f, -1, -1, -1};
            m_renderTarget->setDrawBuffers(drawBuffers, 4);
            ctx->setRenderTarget(m_renderTarget);
            ctx->clear();
            camera.lookAt = camera.position + cameraTargets[f];
            camera.worldUp = worldUps[f];
            CameraManager::updateCamera(camera);
            renderer->probeRenderScene(m_scene, camera);
            //renderer->renderScene(m_scene, camera);
        }
    }

    void ReflectionProbe::prefilter()
    {
        const u32 kViewportWidth = m_prefilteredProbe->m_width;
        const u32 kViewportHeight = m_prefilteredProbe->m_height;
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
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
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = getCurrentGfxCtx();
        u32 kNumMips = 11;
        u32 mipWidth = m_prefilteredProbe->m_width; 
        u32 mipHeight = m_prefilteredProbe->m_height;
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setShader(m_prefilterShader);
        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            m_prefilterRts[mip]->attachColorBuffer(m_prefilteredProbe, mip);
            ctx->setViewport({ 0u, 0u, m_prefilterRts[mip]->m_width, m_prefilterRts[mip]->m_height });
            for (u32 f = 0; f < 6u; f++)
            {
                ctx->setRenderTarget(m_prefilterRts[mip], f);
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniforms
                m_prefilterMatl->set("projection", &camera.projection[0]);
                m_prefilterMatl->set("view", &camera.view[0]);
                m_prefilterMatl->set("roughness", mip * (1.f / (kNumMips - 1)));
                m_prefilterMatl->bindTexture("envmapSampler", m_radianceMap);
                m_prefilterMatl->bind();
                auto va = getMesh("CubeMesh")->m_subMeshes[0]->m_vertexArray;
                ctx->setVertexArray(va);
                ctx->drawIndexAuto(va->numVerts());
            }
            mipWidth /= 2u;
            mipHeight /= 2u;
        }
        ctx->setDepthControl(DepthControl::kEnable);
    }

    IrradianceProbe* LightProbeFactory::createIrradianceProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        auto probe = new IrradianceProbe("IrradianceProbe0", id, position, scene->m_rootEntity, scene);
        scene->entities.push_back(probe);
        return probe;
    }

    ReflectionProbe* LightProbeFactory::createReflectionProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        auto probe = new ReflectionProbe("ReflectionProbe0", id, position, scene->m_rootEntity, scene);
        scene->entities.push_back(probe);
        return probe;
    }
}