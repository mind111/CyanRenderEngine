#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "Asset.h"

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
                srcSpec.type = Texture::Type::TEX_2D;
                srcSpec.dataType = Texture::DataType::Float;
                srcSpec.min = Texture::Filter::LINEAR;
                srcSpec.mag = Texture::Filter::LINEAR;
                srcSpec.s = Texture::Wrap::NONE;
                srcSpec.t = Texture::Wrap::NONE;
                srcSpec.r = Texture::Wrap::NONE;
                srcSpec.numMips = 1u;
                srcSpec.data = nullptr;
                m_srcHDRITexture = textureManager->createTextureHDR("HDRITexture", srcImagePath, srcSpec);
            }
        case kUseProcedural:
        default:
            {
                TextureSpec dstSpec = { };
                dstSpec.type = Texture::Type::TEX_CUBEMAP;
                dstSpec.width = resolution.x;
                dstSpec.height = resolution.y;
                dstSpec.dataType = Texture::DataType::Float;
                dstSpec.min = Texture::Filter::LINEAR;
                dstSpec.mag = Texture::Filter::LINEAR;
                dstSpec.s = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.t = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.r = Texture::Wrap::CLAMP_TO_EDGE;
                dstSpec.numMips = 1u;
                dstSpec.data = nullptr;
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
        // m_renderSkyMatl = createMaterial(s_CubemapSkyShader)->createInstance();
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
                RenderTarget* renderTarget = createRenderTarget(m_srcCubemapTexture->width, m_srcCubemapTexture->height);
                renderTarget->setColorBuffer(m_srcCubemapTexture, 0u);
                Shader* shader = createShader("RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
                // auto    matl = createMaterial(shader)->createInstance();
                Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");
                ctx->setShader(shader);
                // matl->bindTexture("srcImageTexture", m_srcHDRITexture);
                shader->setTexture("srcImageTexture", m_srcHDRITexture);
                for (u32 f = 0; f < 6u; f++)
                {
                    // Update view matrix
                    camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                    camera.worldUp = LightProbeCameras::worldUps[f];
                    camera.update();

                    // matl->set("projection", &camera.projection);
                    // matl->set("view", &camera.view);
                    shader->setUniformMat4("projection", &camera.projection[0].x);
                    shader->setUniformMat4("view", &camera.view[0].x);
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
#if 0
                m_renderSkyMatl->bindTexture("skyDomeTexture", m_srcCubemapTexture);
                m_renderSkyMatl->bind();
#endif
                s_CubemapSkyShader->setTexture("skyDomeTexture", m_srcCubemapTexture);
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