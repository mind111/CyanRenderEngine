#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"

namespace Cyan
{
    Shader* SkyBox::s_renderSkyShader = nullptr;

    SkyBox::SkyBox(const char* srcImagePath, const glm::vec2& resolution)
        : m_srcHDRITexture(nullptr), m_diffuseProbe(nullptr), m_specularProbe(nullptr), m_srcCubemapTexture(nullptr), m_renderSkyMatl(nullptr)
    {
        TextureSpec srcSpec = { };
        srcSpec.m_type = Texture::Type::TEX_2D;
        srcSpec.m_min = Texture::Filter::LINEAR;
        srcSpec.m_mag = Texture::Filter::LINEAR;
        srcSpec.m_s = Texture::Wrap::NONE;
        srcSpec.m_t = Texture::Wrap::NONE;
        srcSpec.m_r = Texture::Wrap::NONE;
        srcSpec.m_numMips = 1u;
        srcSpec.m_data = nullptr;
        srcSpec.m_dataType = Texture::DataType::Float;
        auto textureManager = TextureManager::getSingletonPtr();
        m_srcHDRITexture = textureManager->createTextureHDR("HDRITexture", srcImagePath, srcSpec);

        TextureSpec dstSpec = { };
        dstSpec.m_type = Texture::Type::TEX_2D;
        dstSpec.m_type = Texture::Type::TEX_CUBEMAP;
        dstSpec.m_width = resolution.x;
        dstSpec.m_height = resolution.y;
        dstSpec.m_min = Texture::Filter::LINEAR;
        dstSpec.m_mag = Texture::Filter::LINEAR;
        dstSpec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        dstSpec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        dstSpec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        m_srcCubemapTexture = textureManager->createTextureHDR("SkyBoxTexture", srcSpec);

        auto sceneManager = SceneManager::getSingletonPtr();
        m_diffuseProbe = sceneManager->createIrradianceProbe(m_srcCubemapTexture, glm::uvec2(64u));
        m_specularProbe = sceneManager->createReflectionProbe(m_srcCubemapTexture);
        
        initialize();
    }

    void SkyBox::initialize()
    {
        if (!s_renderSkyShader)
        {
            s_renderSkyShader = createShader("SkyDomeShader", SHADER_SOURCE_PATH "skybox_v.glsl", SHADER_SOURCE_PATH "skybox_p.glsl");
        }
        m_renderSkyMatl = createMaterial(s_renderSkyShader)->createInstance();
    }

    void SkyBox::build()
    {
        auto ctx = getCurrentGfxCtx();
        auto renderer = Renderer::getSingletonPtr();
        Camera camera = { };
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

        // step 1: create a cubemap texture from a src equirectangular image
        {
            RenderTarget* renderTarget = createRenderTarget(m_srcCubemapTexture->m_width, m_srcCubemapTexture->m_height);
            renderTarget->attachColorBuffer(m_srcCubemapTexture);
            Shader* shader = createShader("RenderToCubemapShader", "../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
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
                ctx->setRenderTarget(renderTarget, f);
                ctx->setViewport({ 0, 0, renderTarget->m_width, renderTarget->m_height});
                ctx->setPrimitiveType(PrimitiveType::TriangleList);
                renderer->drawMesh(cubeMesh);
            }
            ctx->setDepthControl(DepthControl::kEnable);
            // release one-time resources
            glDeleteFramebuffers(1, &renderTarget->m_frameBuffer);
            delete renderTarget;
        }
    }

    void SkyBox::buildSkyLight()
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

    void SkyBox::render()
    {
        auto ctx = getCurrentGfxCtx();
        ctx->setShader(s_renderSkyShader);
        m_renderSkyMatl->bindTexture("skyDomeTexture", m_srcCubemapTexture);
        m_renderSkyMatl->bind();
        ctx->drawIndexAuto(36);
    }
}