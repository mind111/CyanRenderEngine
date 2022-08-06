#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "AssetManager.h"

namespace Cyan
{
    Shader* Skybox::s_CubemapSkyShader = nullptr;
    Shader* Skybox::s_proceduralSkyShader = nullptr;

    Skybox::Skybox(Scene* scene, const char* srcImagePath, const glm::vec2& resolution, const SkyboxConfig& cfg)
        : m_cfg(cfg), m_srcHDRITexture(nullptr), m_diffuseProbe(nullptr), m_specularProbe(nullptr), m_srcCubemapTexture(nullptr)
    {
        switch (m_cfg)
        {
        case kUseHDRI:
            {
                ITextureRenderable::Spec spec = { };
                AssetManager::importTexture2D("HDRITexture", srcImagePath, spec);
            }
        case kUseProcedural:
        default:
            {
                ITextureRenderable::Spec spec = { };
                spec.width = resolution.x;
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
                // m_srcCubemapTexture = AssetManager::createTextureCube("SkyboxTexture", spec);
            }
            break;
        }

        m_diffuseProbe = scene->createIrradianceProbe(m_srcCubemapTexture, glm::uvec2(64u));
        m_specularProbe = scene->createReflectionProbe(m_srcCubemapTexture);
        
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

        switch (m_cfg)
        {
        case kUseHDRI:
            // step 1: create a cubemap texture from a src equirectangular image
            {
                RenderTarget* renderTarget = createRenderTarget(m_srcCubemapTexture->resolution, m_srcCubemapTexture->resolution);
                renderTarget->setColorBuffer(m_srcCubemapTexture, 0u);
                Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl" });
                Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");
                for (u32 f = 0; f < 6u; f++)
                {
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
                        [this, f](RenderTarget* renderTarget, Shader* shader) {
                            // Update view matrix
                            PerspectiveCamera camera(
                                glm::vec3(0.f),
                                LightProbeCameras::cameraFacingDirections[f],
                                LightProbeCameras::worldUps[f],
                                glm::radians(90.f),
                                0.1f,
                                100.f,
                                1.0f
                            );
                            shader->setTexture("srcImageTexture", m_srcHDRITexture);
                            shader->setUniform("projection", camera.projection());
                            shader->setUniform("view", camera.view());
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