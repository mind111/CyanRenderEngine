#include "SkyBox.h"
#include "Camera.h"
#include "CyanRenderer.h"
#include "Scene.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "RenderPass.h"
#include "SkyboxComponent.h"

namespace Cyan
{
#if 0
    Skybox::Skybox(const char* m_name, const char* srcHDRIPath, const glm::uvec2& resolution) 
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
        auto HDRIImage = AssetImporter::importImage(srcHDRIPath, srcHDRIPath);
        m_srcHDRITexture = AssetManager::createTexture2D(m_name, HDRIImage, sampler);

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
        StaticMesh* cubeMesh = AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get();

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
                [this, f](ProgramPipeline* p) {
#if 0
                    PerspectiveCamera camera(
                        glm::vec3(0.f), // position
                        LightProbeCameras::cameraFacingDirections[f], // lookAt
                        LightProbeCameras::worldUps[f], // worldUp
                        glm::uvec2(m_cubemapTexture->resolution, m_cubemapTexture->resolution), // renderResolution
                        VM_SCENE_COLOR,
                        90.f, // fov
                        0.1f, // n
                        100.f // f
                    );
                    p->setUniform("projection", camera.projection());
                    p->setUniform("view", camera.view());
                    p->setTexture("srcImageTexture", m_srcHDRITexture->m_gfxTexture.get());
#endif
                },
                gfxPipelineState
            );
        }

        // make sure that the input cubemap resolution is power of 2
        assert(isPowerOf2(m_cubemapTexture->resolution));

        m_cubemapTexture->numMips = (u32)log2f(m_cubemapTexture->resolution) + 1;
        glGenerateTextureMipmap(m_cubemapTexture->getGpuResource());
    }
#endif

    Skybox::Skybox(SkyboxComponent* skyboxComponent)
        : m_skyboxComponent(skyboxComponent)
    {
        if (m_skyboxComponent->m_HDRI != nullptr)
        {
            u32 numMips = std::log2(m_skyboxComponent->m_cubemapResolution);
            GfxTextureCube::Spec spec(m_skyboxComponent->m_cubemapResolution, numMips, PF_RGB16F);
            SamplerCube sampler;
            sampler.minFilter = FM_TRILINEAR;
            sampler.magFilter = FM_BILINEAR;
            sampler.wrapS = WM_CLAMP;
            sampler.wrapT = WM_CLAMP;
            m_cubemap.reset(GfxTextureCube::create(spec, sampler));

            CreateVS(vs, "RenderToCubemapVS", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl");
            CreatePS(ps, "RenderToCubemapPS", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
            CreatePixelPipeline(pipeline, "RenderToCubemap", vs, ps);
            StaticMesh* cubeMesh = AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get();

            for (i32 f = 0; f < 6u; f++)
            {
                GfxPipelineState gfxPipelineState;
                gfxPipelineState.depth = DepthControl::kDisable;

                Renderer::get()->drawStaticMesh(
                    getFramebufferSize(m_cubemap.get()),
                    [this, f](RenderPass& pass) {
                        pass.setRenderTarget(RenderTarget(m_cubemap.get(), f), 0);
                    },
                    { 0, 0, m_cubemap->resolution, m_cubemap->resolution },
                    cubeMesh,
                    pipeline,
                    [this, f](ProgramPipeline* p) {
                        PerspectiveCamera camera;
                        camera.m_position = glm::vec3(0.f);
                        camera.m_worldUp = worldUps[f];
                        camera.m_forward = cameraFacingDirections[f];
                        camera.m_right = glm::cross(camera.m_forward, camera.m_worldUp);
                        camera.m_up = glm::cross(camera.m_right, camera.m_forward);
                        camera.n = .1f;
                        camera.f = 100.f;
                        camera.fov = 90.f;
                        camera.aspectRatio = 1.f;
                        p->setUniform("cameraView", camera.view());
                        p->setUniform("cameraProjection", camera.projection());
                        p->setTexture("srcHDRI", m_skyboxComponent->m_HDRI->getGfxResource());
                    },
                    gfxPipelineState
                );
            }
            glGenerateTextureMipmap(m_cubemap->getGpuResource());
        }
    }

}