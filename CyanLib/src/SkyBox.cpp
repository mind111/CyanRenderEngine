#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "AssetManager.h"

namespace Cyan
{
    Shader* Skybox::s_cubemapSkyShader = nullptr;
    Shader* Skybox::s_proceduralSkyShader = nullptr;

    Skybox::Skybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution) {
        if (!s_proceduralSkyShader) {
            s_proceduralSkyShader = ShaderManager::createShader({ ShaderType::kVsPs, "SDFSkyShader", SHADER_SOURCE_PATH "sky_sdf_v.glsl", SHADER_SOURCE_PATH "sky_sdf_p.glsl" });
        }
        if (!s_cubemapSkyShader) {
            s_cubemapSkyShader = ShaderManager::createShader({ ShaderType::kVsPs, "SkyDomeShader", SHADER_SOURCE_PATH "skybox_v.glsl", SHADER_SOURCE_PATH "skybox_p.glsl" });
        }

        ITextureRenderable::Spec HDRISpec = { };
        HDRISpec.type = TEX_2D;
        m_srcHDRITexture = AssetManager::importTexture2D("HDRITexture", srcHDRIPath, HDRISpec);

        ITextureRenderable::Spec cubemapSpec = { };
        cubemapSpec.type = TEX_CUBE;
        cubemapSpec.width = resolution.x;
        cubemapSpec.height = resolution.x;
        cubemapSpec.pixelFormat = PF_RGB16F;
        ITextureRenderable::Parameter cubemapParams = { };
        cubemapParams.minificationFilter = FM_BILINEAR;
        cubemapParams.magnificationFilter = FM_BILINEAR;
        cubemapParams.wrap_r = WM_CLAMP;
        cubemapParams.wrap_s = WM_CLAMP;
        cubemapParams.wrap_t = WM_CLAMP;

        m_cubemapTexture = AssetManager::createTextureCube(name, cubemapSpec, cubemapParams);

        // render src equirectangular map into a cubemap
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(m_cubemapTexture->resolution, m_cubemapTexture->resolution));
        renderTarget->setColorBuffer(m_cubemapTexture, 0u);

        Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl" });
        Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");

        for (u32 f = 0; f < 6u; f++)
        {
            GfxPipelineState pipelineState;
            pipelineState.depth = DepthControl::kDisable;
            Renderer::get()->submitMesh(
                renderTarget.get(),
                { { (i32)f } },
                false,
                { 0, 0, renderTarget->width, renderTarget->height},
                pipelineState,
                cubeMesh,
                shader,
                [this, f](RenderTarget* renderTarget, Shader* shader) {
                    PerspectiveCamera camera(
                        glm::vec3(0.f),
                        LightProbeCameras::cameraFacingDirections[f],
                        LightProbeCameras::worldUps[f],
                        90.f,
                        0.1f,
                        100.f,
                        1.0f
                    );
                    shader->setTexture("srcImageTexture", m_srcHDRITexture);
                    shader->setUniform("projection", camera.projection());
                    shader->setUniform("view", camera.view());
                });
        }
    }

    Skybox::Skybox(const char* name, TextureCubeRenderable* srcCubemap) {

    }

    void Skybox::render(RenderTarget* renderTarget, const std::initializer_list<RenderTargetDrawBuffer>& drawBuffers)
    {
        Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");

        Renderer::get()->submitMesh(
            renderTarget,
            drawBuffers,
            false,
            { 0, 0, renderTarget->width, renderTarget->height },
            GfxPipelineState(),
            cubeMesh,
            s_cubemapSkyShader,
            [this](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("cubemapTexture", m_cubemapTexture);
            });
    }
}