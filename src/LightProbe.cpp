#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"

namespace Cyan{

    IrradianceProbe::IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(name , id, Transform(), parent), m_scene(scene)
    {
        if (!m_radianceRenderTarget)
        {
            m_radianceRenderTarget = createRenderTarget(512u, 512u);
            m_irradianceRenderTarget = createRenderTarget(64u, 64u);
            m_renderProbeShader = createShader("RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs");
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
        m_radianceMap = createTextureHDR("RadianceProbe", spec);
        // FIXME: this is bugged
        m_radianceRenderTarget->attachColorBuffer(m_radianceMap);
        spec.m_width = 64u;
        spec.m_height = 64u;
        m_irradianceMap = createTextureHDR("IrradianceProbe", spec);
        m_irradianceRenderTarget->attachColorBuffer(m_irradianceMap);

        m_computeIrradianceMatl = createMaterial(m_computeIrradianceShader)->createInstance();
        m_renderProbeMatl = createMaterial(m_renderProbeShader)->createInstance();

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
        ctx->setRenderTarget(m_radianceRenderTarget, 0u);
        ctx->clear();
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

    IrradianceProbe* LightProbeFactory::createIrradianceProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        return new IrradianceProbe("IrradianceProbe0", id, position, nullptr, scene);
    }
}