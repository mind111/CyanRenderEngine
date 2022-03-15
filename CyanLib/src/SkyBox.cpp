#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"

namespace Cyan
{
    Shader* Skybox::s_CubemapSkyShader     = nullptr;
    Shader* Skybox::s_proceduralSkyShader = nullptr;

    Skybox::Skybox(const char* srcImagePath, const glm::vec2& resolution, const SkyboxConfig& cfg)
        : m_cfg(cfg), m_srcHDRITexture(nullptr), m_diffuseProbe(nullptr), m_specularProbe(nullptr), m_srcCubemapTexture(nullptr), m_renderSkyMatl(nullptr)
    {
        auto textureManager = TextureManager::getSingletonPtr();
        switch (m_cfg)
        {
        case kUseHDRI: 
            {
                TextureSpec srcSpec = { };
                srcSpec.m_type = Texture::Type::TEX_2D;
                srcSpec.m_dataType = Texture::DataType::Float;
                srcSpec.m_min = Texture::Filter::LINEAR;
                srcSpec.m_mag = Texture::Filter::LINEAR;
                srcSpec.m_s = Texture::Wrap::NONE;
                srcSpec.m_t = Texture::Wrap::NONE;
                srcSpec.m_r = Texture::Wrap::NONE;
                srcSpec.m_numMips = 1u;
                srcSpec.m_data = nullptr;
                m_srcHDRITexture = textureManager->createTextureHDR("HDRITexture", srcImagePath, srcSpec);
            }
        case kUseProcedural:
        default:
            {
                TextureSpec dstSpec = { };
                dstSpec.m_type = Texture::Type::TEX_CUBEMAP;
                dstSpec.m_width = resolution.x;
                dstSpec.m_height = resolution.y;
                dstSpec.m_dataType = Texture::DataType::Float;
                dstSpec.m_min = Texture::Filter::LINEAR;
                dstSpec.m_mag = Texture::Filter::LINEAR;
                dstSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.m_numMips = 1u;
                dstSpec.m_data = nullptr;
                m_srcCubemapTexture = textureManager->createTextureHDR("SkyBoxTexture", dstSpec);

            }
            break;
        }

        auto sceneManager = SceneManager::getSingletonPtr();
        m_diffuseProbe = sceneManager->createIrradianceProbe(m_srcCubemapTexture, glm::uvec2(64u));
        m_specularProbe = sceneManager->createReflectionProbe(m_srcCubemapTexture);
        
        initialize();
    }

    void Skybox::initialize()
    {
        if (!s_proceduralSkyShader)
        {
            s_proceduralSkyShader = createShader("SDFSkyShader", SHADER_SOURCE_PATH "sky_sdf_v.glsl", SHADER_SOURCE_PATH "sky_sdf_p.glsl");
        }
        if (!s_CubemapSkyShader)
        {
            s_CubemapSkyShader = createShader("SkyDomeShader", SHADER_SOURCE_PATH "skybox_v.glsl", SHADER_SOURCE_PATH "skybox_p.glsl");
        }
        m_renderSkyMatl = createMaterial(s_CubemapSkyShader)->createInstance();
    }

    void Skybox::build()
    {
        auto ctx = getCurrentGfxCtx();
        auto renderer = Renderer::getSingletonPtr();
        Camera camera = { };
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

        switch (m_cfg)
        {
        case kUseHDRI:
            // step 1: create a cubemap texture from a src equirectangular image
            {
                RenderTarget* renderTarget = createRenderTarget(m_srcCubemapTexture->m_width, m_srcCubemapTexture->m_height);
                renderTarget->setColorBuffer(m_srcCubemapTexture, 0u);
                Shader* shader = createShader("RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
                auto    matl = createMaterial(shader)->createInstance();
                Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
                ctx->setShader(shader);
                matl->bindTexture("srcImageTexture", m_srcHDRITexture);
                for (u32 f = 0; f < 6u; f++)
                {
                    // Update view matrix
                    camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                    camera.worldUp = LightProbeCameras::worldUps[f];
                    CameraManager::updateCamera(camera);
                    matl->set("projection", &camera.projection);
                    matl->set("view", &camera.view);
                    ctx->setDepthControl(DepthControl::kDisable);
                    ctx->setRenderTarget(renderTarget, { (i32)f });
                    ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height});
                    ctx->setPrimitiveType(PrimitiveType::TriangleList);
                    renderer->drawMesh(cubeMesh);
                }
                ctx->setDepthControl(DepthControl::kEnable);
                // release one-time resources
                glDeleteFramebuffers(1, &renderTarget->fbo);
                delete renderTarget;
            }
            break;
        // sdf sky is rendered in real-time in contrast to HDRI Skybox that is built by render a src HDRI image
        // into a cubemap texture
        case kUseProcedural:
        default:
            break;
        }
    }

    void Skybox::buildSkyLight()
    {
        // step 2: convolve src cubemap texture into irradiance map
        {
            m_diffuseProbe->buildFromCubemap();
        }

        // step 3: convolve src cubemap texture into glossy reflection map
        {
            m_specularProbe->buildFromCubemap();
        }
    }

    void Skybox::render()
    {
        auto ctx = getCurrentGfxCtx();
        switch (m_cfg)
        {
        case kUseHDRI:
            {
                ctx->setShader(s_CubemapSkyShader);
                m_renderSkyMatl->bindTexture("skyDomeTexture", m_srcCubemapTexture);
                m_renderSkyMatl->bind();
                ctx->drawIndexAuto(36);
            }
            break;
        case kUseProcedural:
            {
                ctx->setShader(s_proceduralSkyShader);
                ctx->drawIndexAuto(36);
            }
            break;
        default:
            break;
        }
    }
}