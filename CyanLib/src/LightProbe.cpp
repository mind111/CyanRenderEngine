#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "stb_image.h"

namespace Cyan
{
    Mesh*          s_cubeMesh                                  = nullptr;
    Shader*        s_debugRenderShader                         = nullptr;
    Shader*        IrradianceProbe::m_convolveIrradianceShader = nullptr;
    Shader*        ReflectionProbe::s_convolveReflectionShader = nullptr;
    RegularBuffer* IrradianceProbe::m_rayBuffers               = nullptr;

    namespace CameraSettings
    {
        const static glm::vec3 cameraFacingDirections[] = {
            {1.f, 0.f, 0.f},   // Right
            {-1.f, 0.f, 0.f},  // Left
            {0.f, 1.f, 0.f},   // Up
            {0.f, -1.f, 0.f},  // Down
            {0.f, 0.f, 1.f},   // Front
            {0.f, 0.f, -1.f},  // Back
        }; 

        const static glm::vec3 worldUps[] = {
            {0.f, -1.f, 0.f},   // Right
            {0.f, -1.f, 0.f},   // Left
            {0.f, 0.f, 1.f},    // Up
            {0.f, 0.f, -1.f},   // Down
            {0.f, -1.f, 0.f},   // Forward
            {0.f, -1.f, 0.f},   // Back
        };
    }

    Shader* getRenderProbeShader()
    {
        if (!s_debugRenderShader)
        {
            s_debugRenderShader = createShader("RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs");
        }
        return s_debugRenderShader;
    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : m_scene(scene), m_position(p), m_resolution(resolution), m_debugSphereMesh(nullptr), m_sceneCapture(nullptr), m_debugRenderMatl(nullptr)
    {
        m_debugSphereMesh = getMesh("sphere_mesh")->createInstance(m_scene);
        m_debugRenderMatl = createMaterial(s_debugRenderShader)->createInstance();

        initialize();
    }

    void LightProbe::initialize()
    {
        TextureSpec spec = { };
        spec.m_width = m_resolution.x;
        spec.m_height = m_resolution.y;
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
        m_sceneCapture = textureManager->createTextureHDR("SceneCapture", spec);
    }

    void LightProbe::captureScene()
    {
        // create render target
        auto renderTarget = createRenderTarget(m_sceneCapture->m_width, m_sceneCapture->m_height);
        {
            renderTarget->attachColorBuffer(m_sceneCapture);
            Camera camera = { };
            camera.position = m_position;
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
            camera.n = 0.1f;
            camera.f = 100.f;
            camera.fov = glm::radians(90.f);
            // render sun light shadow
            auto renderer = Renderer::getSingletonPtr();
            renderer->beginRender();
            renderer->addDirectionalShadowPass(m_scene, camera, 0);
            renderer->render(m_scene);
            renderer->endRender();
            // render scene into each face of the cubemap
            auto ctx = Cyan::getCurrentGfxCtx();
            ctx->setViewport({0u, 0u, m_sceneCapture->m_width, m_sceneCapture->m_height});
            for (u32 f = 0; f < (sizeof(CameraSettings::cameraFacingDirections)/sizeof(CameraSettings::cameraFacingDirections[0])); ++f)
            {
                i32 drawBuffers[4] = {(i32)f, -1, -1, -1};
                renderTarget->setDrawBuffers(drawBuffers, 4);
                ctx->setRenderTarget(renderTarget);
                ctx->clear();

                camera.lookAt = camera.position + CameraSettings::cameraFacingDirections[f];
                camera.worldUp = CameraSettings::worldUps[f];
                CameraManager::updateCamera(camera);
                renderer->renderSceneToLightProbe(m_scene, camera);
            }
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
        delete renderTarget;
    }

    void LightProbe::debugRender()
    {

    }

    IrradianceProbe::IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceRes)
        : LightProbe(scene, p, sceneCaptureResolution), m_irradianceTextureRes(irradianceRes)
    {
        TextureSpec spec = { };
        spec.m_width = 64u;
        spec.m_height = 64u;
        spec.m_type = Texture::Type::TEX_CUBEMAP;
        spec.m_format = Texture::ColorFormat::R16G16B16;
        spec.m_dataType =  Texture::Float;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_numMips = 1u;
        spec.m_data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        auto sceneManager = SceneManager::getSingletonPtr();
        m_convolvedIrradianceTexture = textureManager->createTextureHDR("IrradianceProbe", spec);

        m_convolveIrradianceMatl = createMaterial(m_convolveIrradianceShader)->createInstance();
    }

    void IrradianceProbe::convolve()
    {
        Toolkit::GpuTimer timer("ConvolveIrradianceTimer");
        auto ctx = Cyan::getCurrentGfxCtx();
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        auto renderer = Renderer::getSingletonPtr();
        auto renderTarget = createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y);
        {
            ctx->setShader(m_convolveIrradianceShader);
            ctx->setViewport({0u, 0u, renderTarget->m_width, renderTarget->m_height});
            ctx->setDepthControl(DepthControl::kDisable);
            for (u32 f = 0; f < 6u; ++f)
            {
                camera.lookAt = CameraSettings::cameraFacingDirections[f];
                camera.view = glm::lookAt(camera.position, camera.lookAt, CameraSettings::worldUps[f]);
                ctx->setRenderTarget(renderTarget, f);
                m_convolveIrradianceMatl->set("view", &camera.view[0][0]);
                m_convolveIrradianceMatl->set("projection", &camera.projection[0][0]);
                m_convolveIrradianceMatl->bindTexture("envmapSampler", m_sceneCapture);
                renderer->drawMesh(s_cubeMesh);
            }
            ctx->setDepthControl(DepthControl::kEnable);
            timer.end();
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
        delete renderTarget;
    }

    void IrradianceProbe::debugRender()
    {
    }

    ReflectionProbe::ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution)
        : LightProbe(scene, p, sceneCaptureResolution), m_convolvedReflectionTexture(nullptr), m_convolveReflectionMatl(nullptr)
    {
        if (!s_convolveReflectionShader)
        {
            s_convolveReflectionShader = createShader("ConvolveReflectionShader", "../../shader/shader_prefilter_specular.vs", "../../shader/shader_prefilter_specular.fs");
        }

        // convolved radiance texture
        TextureSpec spec = { };
        spec.m_width = m_resolution.x;
        spec.m_height = m_resolution.y;
        spec.m_type = Texture::Type::TEX_CUBEMAP;
        spec.m_format = Texture::ColorFormat::R16G16B16;
        spec.m_dataType = Texture::Float;
        spec.m_min = Texture::Filter::MIPMAP_LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_numMips = kNumMips;
        spec.m_data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        m_convolvedReflectionTexture = textureManager->createTextureHDR("ConvolvedReflectionProbe", spec);
    }

    void ReflectionProbe::convolve()
    {
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        camera.n = 0.1f;
        camera.f = 100.f;
        camera.fov = glm::radians(90.f);
        auto renderer = Renderer::getSingletonPtr();
        auto ctx = getCurrentGfxCtx();
        u32 kNumMips = 11;
        u32 mipWidth = m_sceneCapture->m_width; 
        u32 mipHeight = m_sceneCapture->m_height;
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setShader(s_convolveReflectionShader);
        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            auto renderTarget = createRenderTarget(mipWidth, mipHeight);
            renderTarget->attachColorBuffer(m_convolvedReflectionTexture, mip);
            ctx->setViewport({ 0u, 0u, renderTarget->m_width, renderTarget->m_height });
            {
                for (u32 f = 0; f < 6u; f++)
                {
                    ctx->setRenderTarget(renderTarget, f);
                    camera.lookAt = CameraSettings::cameraFacingDirections[f];
                    camera.worldUp = CameraSettings::worldUps[f];
                    CameraManager::updateCamera(camera);
                    m_convolveReflectionMatl->set("projection", &camera.projection[0]);
                    m_convolveReflectionMatl->set("view", &camera.view[0]);
                    m_convolveReflectionMatl->set("roughness", mip * (1.f / (kNumMips - 1)));
                    m_convolveReflectionMatl->bindTexture("envmapSampler", m_sceneCapture);
                    m_convolveReflectionMatl->bind();
                    renderer->drawMesh(s_cubeMesh);
                }
            }
            glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
            delete renderTarget;

            mipWidth /= 2u;
            mipHeight /= 2u;
        }
        ctx->setDepthControl(DepthControl::kEnable);
    }

    void ReflectionProbe::bake()
    {
        captureScene();
        convolve();
    }

    void ReflectionProbe::debugRender()
    {

    }

    IrradianceProbe* LightProbeFactory::createIrradianceProbe(Scene* scene, const glm::vec3& position, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceRes)
    {
        auto probe = new IrradianceProbe(scene, position, sceneCaptureRes, irradianceRes);
        return probe;
    }

    ReflectionProbe* LightProbeFactory::createReflectionProbe(Scene* scene, const glm::vec3& position, const glm::uvec2& sceneCaptureRes)
    {
        auto probe = new ReflectionProbe(scene, position, sceneCaptureRes);
        return probe;
    }
}