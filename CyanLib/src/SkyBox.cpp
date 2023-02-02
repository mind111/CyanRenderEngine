#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "AssetManager.h"

namespace Cyan
{
    PixelPipeline* Skybox::s_cubemapSkyPipeline = nullptr;
    PixelPipeline* Skybox::s_proceduralSkyPipeline = nullptr;

    Skybox::Skybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution) 
    {
        if (!s_proceduralSkyPipeline) 
        {
            CreateVS(vs, "SDFSkyVS", SHADER_SOURCE_PATH "sky_sdf_v.glsl");
            CreatePS(ps, "SDFSkyPS", SHADER_SOURCE_PATH "sky_sdf_p.glsl");
            s_proceduralSkyPipeline = ShaderManager::createPixelPipeline("SDFSky", vs, ps);
        }
        if (!s_cubemapSkyPipeline) 
        {
            CreateVS(vs, "SkyDomeVS", SHADER_SOURCE_PATH "skybox_v.glsl");
            CreatePS(ps, "SkyDomePS", SHADER_SOURCE_PATH "skybox_p.glsl");
            s_cubemapSkyPipeline = ShaderManager::createPixelPipeline("SkyDome", vs, ps);
        }

        // todo: parse out the name of the HDRI and use that as the texture name 
        Sampler2D sampler;
        sampler.minFilter = FM_BILINEAR;
        sampler.magFilter = FM_BILINEAR;
        m_srcHDRITexture = AssetManager::importTexture2D("SkyboxHDRI", srcHDRIPath, false, sampler);

        u32 numMips = log2(resolution.x) + 1;
        TextureCube::Spec cubemapSpec(resolution.x, numMips, PF_RGB16F);
        SamplerCube samplerCube;
        samplerCube.minFilter = FM_TRILINEAR;
        samplerCube.magFilter = FM_BILINEAR;
        samplerCube.wrapS = WM_CLAMP;
        samplerCube.wrapT = WM_CLAMP;

        m_cubemapTexture = std::make_unique<TextureCube>(name, cubemapSpec, samplerCube);
        m_cubemapTexture->init();

        // render src equirectangular map into a cubemap
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(m_cubemapTexture->resolution, m_cubemapTexture->resolution));
        renderTarget->setColorBuffer(m_cubemapTexture.get(), 0u);

        CreateVS(vs, "RenderToCubemapVS", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl");
        CreatePS(ps, "RenderToCubemapPS", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "RenderToCubemap", vs, ps);
        Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");

        for (i32 f = 0; f < 6u; f++)
        {
            renderTarget->setDrawBuffers({ f });
            renderTarget->clear({ { f } });

            GfxPipelineConfig config;
            config.depth = DepthControl::kDisable;
            Renderer::get()->drawMesh(
                renderTarget.get(),
                { 0, 0, renderTarget->width, renderTarget->height },
                cubeMesh,
                pipeline,
                [this, f](VertexShader* vs, PixelShader* ps) {
                    PerspectiveCamera camera(
                        glm::vec3(0.f),
                        LightProbeCameras::cameraFacingDirections[f],
                        LightProbeCameras::worldUps[f],
                        90.f,
                        0.1f,
                        100.f,
                        1.0f
                    );
                    vs->setUniform("projection", camera.projection());
                    vs->setUniform("view", camera.view());
                    ps->setTexture("srcImageTexture", m_srcHDRITexture);
                },
                config
            );
        }

        // make sure that the input cubemap resolution is power of 2
        if (m_cubemapTexture->resolution & (m_cubemapTexture->resolution - 1) != 0) {
            assert(0);
        }
        m_cubemapTexture->numMips = (u32)log2f(m_cubemapTexture->resolution) + 1;
        glGenerateTextureMipmap(m_cubemapTexture->getGpuObject());
    }

    
    Skybox::Skybox(const char* name, TextureCube* srcCubemap) 
    {

    }

    void Skybox::render(RenderTarget* renderTarget, const glm::mat4& view, const glm::mat4& projection, f32 mipLevel)
    {
        Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");

        Renderer::get()->drawMesh(
            renderTarget,
            { 0, 0, renderTarget->width, renderTarget->height },
            cubeMesh,
            s_cubemapSkyPipeline,
            [this, mipLevel, view, projection](VertexShader* vs, PixelShader* ps) {
                vs->setUniform("view", view);
                vs->setUniform("projection", projection);
                ps->setUniform("mipLevel", mipLevel);
                ps->setTexture("cubemapTexture", m_cubemapTexture.get());
            });
    }
}