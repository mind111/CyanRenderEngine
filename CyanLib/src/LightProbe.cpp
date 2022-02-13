#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "stb_image.h"

namespace Cyan
{
    Mesh*          s_cubeMesh                                  = nullptr;
    Shader*        s_debugRenderShader                         = nullptr;
    Texture*       ReflectionProbe::s_BRDFLookupTexture        = nullptr;
    Shader*        IrradianceProbe::s_convolveIrradianceShader = nullptr;
    Shader*        ReflectionProbe::s_convolveReflectionShader = nullptr;
    RegularBuffer* IrradianceProbe::m_rayBuffers               = nullptr;

    namespace LightProbeCameras
    {
        const glm::vec3 cameraFacingDirections[] = {
            {1.f, 0.f, 0.f},   // Right
            {-1.f, 0.f, 0.f},  // Left
            {0.f, 1.f, 0.f},   // Up
            {0.f, -1.f, 0.f},  // Down
            {0.f, 0.f, 1.f},   // Front
            {0.f, 0.f, -1.f},  // Back
        }; 

        const glm::vec3 worldUps[] = {
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

    LightProbe::LightProbe(Texture* srcCubemapTexture)
        : m_scene(nullptr), m_position(glm::vec3(0.f)), m_resolution(glm::uvec2(srcCubemapTexture->m_width, srcCubemapTexture->m_height)), 
        m_debugSphereMesh(nullptr), m_sceneCapture(srcCubemapTexture), m_debugRenderMatl(nullptr)
    {

    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : m_scene(scene), m_position(p), m_resolution(resolution), m_debugSphereMesh(nullptr), m_sceneCapture(nullptr), m_debugRenderMatl(nullptr)
    {
        m_debugSphereMesh = getMesh("sphere_mesh")->createInstance(m_scene);
        m_debugRenderMatl = createMaterial(getRenderProbeShader())->createInstance();

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

        // shared cube mesh
        if (!s_cubeMesh)
        {
            auto created = (getMesh("CubeMesh") != nullptr);
            if (!created)
            {
                s_cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            else
            {
                s_cubeMesh = getMesh("CubeMesh");
            }
        }
    }

    void LightProbe::captureScene()
    {
        CYAN_ASSERT(m_scene, "Attempt to call LightProbe::captureScene() while scene is NULL!");

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
            // only capture static objects
            std::vector<Entity*> staticObjects;
            for (u32 i = 0; i < m_scene->entities.size(); ++i)
            {
                if (m_scene->entities[i]->m_static)
                {
                    staticObjects.push_back(m_scene->entities[i]);
                }
            }
            renderer->beginRender();
            renderer->addDirectionalShadowPass(m_scene, m_scene->getActiveCamera(), staticObjects);
            renderer->render(m_scene);
            renderer->endRender();
            // render scene into each face of the cubemap
            auto ctx = Cyan::getCurrentGfxCtx();
            ctx->setViewport({0u, 0u, m_sceneCapture->m_width, m_sceneCapture->m_height});
            for (u32 f = 0; f < (sizeof(LightProbeCameras::cameraFacingDirections)/sizeof(LightProbeCameras::cameraFacingDirections[0])); ++f)
            {
                i32 drawBuffers[4] = {(i32)f, -1, -1, -1};
                renderTarget->setDrawBuffers(drawBuffers, 4);
                ctx->setRenderTarget(renderTarget);
                ctx->clear();

                camera.lookAt = camera.position + LightProbeCameras::cameraFacingDirections[f];
                camera.worldUp = LightProbeCameras::worldUps[f];
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

    IrradianceProbe::IrradianceProbe(Texture* srcCubemapTexture, const glm::uvec2& irradianceRes)
        : LightProbe(srcCubemapTexture), m_irradianceTextureRes(irradianceRes)
    {
        initialize();
    }

    IrradianceProbe::IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceRes)
        : LightProbe(scene, p, sceneCaptureResolution), m_irradianceTextureRes(irradianceRes)
    {
        initialize();
    }

    void IrradianceProbe::initialize()
    {
        TextureSpec spec = { };
        spec.m_width = m_irradianceTextureRes.x;
        spec.m_height = m_irradianceTextureRes.y;
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

        if (!s_convolveIrradianceShader)
        {
            s_convolveIrradianceShader = createShader("ConvolveIrradianceShader", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl");
        }
        m_convolveIrradianceMatl = createMaterial(s_convolveIrradianceShader)->createInstance();
    }

    void IrradianceProbe::convolve()
    {
        Toolkit::GpuTimer timer("ConvolveIrradianceTimer");
        auto ctx = Cyan::getCurrentGfxCtx();
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        auto renderTarget = createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y);
        renderTarget->attachColorBuffer(m_convolvedIrradianceTexture);
        {
            ctx->setShader(s_convolveIrradianceShader);
            ctx->setViewport({0u, 0u, renderTarget->m_width, renderTarget->m_height});
            ctx->setDepthControl(DepthControl::kDisable);
            for (u32 f = 0; f < 6u; ++f)
            {
                camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                camera.worldUp = LightProbeCameras::worldUps[f];
                CameraManager::updateCamera(camera);
                ctx->setRenderTarget(renderTarget, f);
                m_convolveIrradianceMatl->set("view", &camera.view[0]);
                m_convolveIrradianceMatl->set("projection", &camera.projection[0]);
                m_convolveIrradianceMatl->bindTexture("srcCubemapTexture", m_sceneCapture);
                m_convolveIrradianceMatl->bind();
                ctx->drawIndexAuto(36u);
            }
            ctx->setDepthControl(DepthControl::kEnable);
            timer.end();
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
        delete renderTarget;
    }

    void IrradianceProbe::build()
    {
        captureScene();
        convolve();
    }

    void IrradianceProbe::buildFromCubemap()
    {
        convolve();
    }

    void IrradianceProbe::debugRender()
    {

    }

    ReflectionProbe::ReflectionProbe(Texture* srcCubemapTexture)
        : LightProbe(srcCubemapTexture)
    {
        initialize();
    }

    ReflectionProbe::ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution)
        : LightProbe(scene, p, sceneCaptureResolution), m_convolvedReflectionTexture(nullptr), m_convolveReflectionMatl(nullptr)
    {
        initialize();
    }

    void ReflectionProbe::initialize()
    {
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

        if (!s_convolveReflectionShader)
        {
            s_convolveReflectionShader = createShader("ConvolveReflectionShader", SHADER_SOURCE_PATH "convolve_specular_v.glsl", SHADER_SOURCE_PATH "convolve_specular_p.glsl");
        }
        m_convolveReflectionMatl = createMaterial(s_convolveReflectionShader)->createInstance();

        // generate shared BRDF lookup texture
        if (!s_BRDFLookupTexture)
        {
            s_BRDFLookupTexture = buildBRDFLookupTexture();
        }
    }

    Texture* ReflectionProbe::buildBRDFLookupTexture()
    {
        const u32 kTexWidth = 512u;
        const u32 kTexHeight = 512u;
        TextureSpec spec = { };
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16;
        spec.m_dataType = Texture::DataType::Float;
        spec.m_numMips = 1u;
        spec.m_width = kTexWidth;
        spec.m_height = kTexHeight;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        Texture* outTexture = textureManager->createTextureHDR("BRDFLUT", spec); 
        Shader* shader = createShader("IntegrateBRDFShader", "../../shader/shader_integrate_brdf.vs", "../../shader/shader_integrate_brdf.fs");
        RenderTarget* rt = createRenderTarget(kTexWidth, kTexWidth);
        rt->attachColorBuffer(outTexture);
        f32 verts[] = {
            -1.f,  1.f, 0.f, 0.f,  1.f,
            -1.f, -1.f, 0.f, 0.f,  0.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
            -1.f,  1.f, 0.f, 0.f,  1.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
             1.f,  1.f, 0.f, 1.f,  1.f
        };
        GLuint vbo, vao;
        glCreateBuffers(1, &vbo);
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glNamedBufferData(vbo, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexArrayAttrib(vao, 0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
        glEnableVertexArrayAttrib(vao, 1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        auto ctx = getCurrentGfxCtx();
        ctx->setViewport({ 0, 0, kTexWidth, kTexHeight } );
        ctx->setShader(shader);
        ctx->setRenderTarget(rt, 0);
        glBindVertexArray(vao);
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        ctx->reset();
        glDeleteFramebuffers(1, &rt->m_frameBuffer);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        return outTexture;
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
                    camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                    camera.worldUp = LightProbeCameras::worldUps[f];
                    CameraManager::updateCamera(camera);
                    m_convolveReflectionMatl->set("projection", &camera.projection[0]);
                    m_convolveReflectionMatl->set("view", &camera.view[0]);
                    m_convolveReflectionMatl->set("roughness", mip * (1.f / (kNumMips - 1)));
                    m_convolveReflectionMatl->bindTexture("envmapSampler", m_sceneCapture);
                    m_convolveReflectionMatl->bind();
                    ctx->drawIndexAuto(36);
                }
            }
            glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
            delete renderTarget;

            mipWidth /= 2u;
            mipHeight /= 2u;
        }
        ctx->setDepthControl(DepthControl::kEnable);
    }

    void ReflectionProbe::build()
    {
        captureScene();
        convolve();
    }

    void ReflectionProbe::buildFromCubemap()
    {
        convolve();
    }

    void ReflectionProbe::debugRender()
    {

    }
}