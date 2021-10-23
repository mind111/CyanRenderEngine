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

    LightFieldProbe::LightFieldProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene)
        : Entity(name , id, Transform(), parent), m_scene(scene)
    {
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
            m_octMapRenderTarget->attachTexture(m_radianceOct, 0u);
            m_octMapRenderTarget->attachTexture(m_normalOct, 1u);
            m_octMapRenderTarget->attachTexture(m_distanceOct, 2u);
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
    
    void LightFieldProbe::sampleRadianceOctMap()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // sample radiance into a cubemap
        {
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
            ctx->setViewport({ 0u, 0u, m_cubemapRenderTarget->m_width, m_cubemapRenderTarget->m_height });
            for (u32 f = 0; f < (sizeof(cameraTargets)/sizeof(cameraTargets[0])); ++f)
            {
                // rebind color attachments
                {
                    m_cubemapRenderTarget->attachTexture(m_radiance, 0, f);
                    m_cubemapRenderTarget->attachTexture(m_normal, 1, f);
                    m_cubemapRenderTarget->attachTexture(m_radialDistance, 2, f);
                }
                u32 drawBuffers[4] = { 0, 1, -1, 2 };
                ctx->setRenderTarget(m_cubemapRenderTarget, drawBuffers, 4u);
                ctx->clear();
                camera.lookAt = cameraTargets[f];
                camera.view = glm::lookAt(camera.position, camera.position + cameraTargets[f], worldUps[f]);
                Renderer::getSingletonPtr()->renderScene(m_scene, camera);
            }
        }
        
        // resample radiance cubemap into a octmap using octahedral projection
        {
            ctx->setDepthControl(DepthControl::kDisable);
            u32 drawBuffers[3] = { 0, 1, 2 };
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

    IrradianceProbe* LightProbeFactory::createIrradianceProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        return new IrradianceProbe("IrradianceProbe0", id, position, nullptr, scene);
    }

    LightFieldProbe* LightProbeFactory::createLightFieldProbe(Scene* scene, glm::vec3 position)
    {
        auto sceneManager = SceneManager::getSingletonPtr();
        u32 id = sceneManager->allocEntityId(scene);
        return new LightFieldProbe("LightFieldProbe", id, position, nullptr, scene);
    }
}