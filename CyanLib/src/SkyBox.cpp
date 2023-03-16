#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "RenderPass.h"

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
        // m_srcHDRITexture = AssetManager::importTexture2D("SkyboxHDRI", srcHDRIPath, sampler);
        auto HDRIImage = AssetImporter::importImageSync(srcHDRIPath, srcHDRIPath);
        m_srcHDRITexture = AssetManager::createTexture2D(name, HDRIImage, sampler);

        u32 numMips = log2(resolution.x) + 1;
        GfxTextureCube::Spec cubemapSpec(resolution.x, numMips, PF_RGB16F);
        SamplerCube samplerCube;
        samplerCube.minFilter = FM_TRILINEAR;
        samplerCube.magFilter = FM_BILINEAR;
        samplerCube.wrapS = WM_CLAMP;
        samplerCube.wrapT = WM_CLAMP;

        m_cubemapTexture = std::unique_ptr<GfxTextureCube>(GfxTextureCube::create(cubemapSpec, samplerCube));

        CreateVS(vs, "RenderToCubemapVS", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl");
        CreatePS(ps, "RenderToCubemapPS", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "RenderToCubemap", vs, ps);
        StaticMesh* cubeMesh = AssetManager::getAsset<StaticMesh>("UnitCubeMesh");

        for (i32 f = 0; f < 6u; f++)
        {
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;
            Renderer::get()->drawStaticMesh(
                getFramebufferSize(m_cubemapTexture.get()),
                [this, f](RenderPass& pass) {
                    pass.setRenderTarget(RenderTarget(m_cubemapTexture.get(), f), 0);
                },
                { 0, 0, m_cubemapTexture->resolution, m_cubemapTexture->resolution },
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
                    ps->setTexture("srcImageTexture", m_srcHDRITexture->gfxTexture.get());
                },
                gfxPipelineState
            );
        }

        // make sure that the input cubemap resolution is power of 2
        if (m_cubemapTexture->resolution & (m_cubemapTexture->resolution - 1) != 0) 
        {
            assert(0);
        }
        m_cubemapTexture->numMips = (u32)log2f(m_cubemapTexture->resolution) + 1;
        glGenerateTextureMipmap(m_cubemapTexture->getGpuResource());
    }

    
    Skybox::Skybox(const char* name, GfxTextureCube* srcCubemap) 
    {

    }

    void Skybox::render(Framebuffer* framebuffer, const glm::mat4& view, const glm::mat4& projection, f32 mipLevel)
    {
        StaticMesh* cubeMesh = AssetManager::getAsset<StaticMesh>("UnitCubeMesh");

        Renderer::get()->drawStaticMesh(
            getFramebufferSize(m_cubemapTexture.get()),
            []() {

            },
            { 0, 0, framebuffer->width, framebuffer->height },
            cubeMesh,
            s_cubemapSkyPipeline,
            [this, mipLevel, view, projection](VertexShader* vs, PixelShader* ps) {
                vs->setUniform("view", view);
                vs->setUniform("projection", projection);
                ps->setUniform("mipLevel", mipLevel);
                ps->setTexture("cubemapTexture", m_cubemapTexture.get());
            },
            GfxPipelineState { }
        );
    }
}