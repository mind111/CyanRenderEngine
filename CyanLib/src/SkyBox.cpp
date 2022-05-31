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
        : m_cfg(cfg), m_srcHDRITexture(nullptr), m_diffuseProbe(nullptr), m_specularProbe(nullptr), m_srcCubemapTexture(nullptr)
    {
        auto textureManager = TextureManager::get();
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

        auto sceneManager = SceneManager::get();
        m_diffuseProbe = sceneManager->createIrradianceProbe(m_srcCubemapTexture, glm::uvec2(64u));
        m_specularProbe = sceneManager->createReflectionProbe(m_srcCubemapTexture);
        
        initialize();
    }

    void Skybox::initialize()
    {
        if (!s_proceduralSkyShader)
        {
            s_proceduralSkyShader = ShaderManager::createShader({ ShaderType::kVsPs, "SDFSkyShader", SHADER_SOURCE_PATH "sky_sdf_v.glsl", SHADER_SOURCE_PATH "sky_sdf_p.glsl" });
        }
        if (!s_CubemapSkyShader)
        {
            s_CubemapSkyShader = ShaderManager::createShader({ ShaderType::kVsPs, "SkyDomeShader", SHADER_SOURCE_PATH "skybox_v.glsl", SHADER_SOURCE_PATH "skybox_p.glsl" });
        }
    }

    void Skybox::build()
    {
        auto renderer = Renderer::get();
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
                Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl" });
                Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");
                for (u32 f = 0; f < 6u; f++)
                {
                    // Update view matrix
                    camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                    camera.worldUp = LightProbeCameras::worldUps[f];
                    camera.update();

                    GfxPipelineState pipelineState;
                    pipelineState.depth = DepthControl::kDisable;
                    renderer->submitMesh(
                        renderTarget,
                        { { (i32)f } },
                        false,
                        { 0, 0, renderTarget->width, renderTarget->height},
                        pipelineState,
                        cubeMesh,
                        shader,
                        [this, camera](RenderTarget* renderTarget, Shader* shader) {
                            shader->setTexture("srcImageTexture", m_srcHDRITexture);
                            shader->setUniform("projection", camera.projection);
                            shader->setUniform("view", camera.view);
                        });
                }
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
        auto ctx = GfxContext::get();
        switch (m_cfg)
        {
        case kUseHDRI:
            {
                ctx->setShader(s_CubemapSkyShader);
                s_CubemapSkyShader->setTexture("skyDomeTexture", m_srcCubemapTexture);
                s_CubemapSkyShader->commit(ctx);
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