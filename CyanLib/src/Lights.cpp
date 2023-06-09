#include "Lights.h"
#include "LightComponents.h"
#include "Shader.h"
#include "AssetManager.h"
#include "GfxContext.h"
#include "CyanRenderer.h"
#include "RenderPass.h"

namespace Cyan 
{
    Light::Light(const glm::vec3& color, f32 intensity)
        : m_color(color), m_intensity(intensity)
    {

    }

    Light::~Light()
    {

    }

    DirectionalLight::DirectionalLight(DirectionalLightComponent* directionalLightComponent)
        : Light(directionalLightComponent->getColor(), directionalLightComponent->getIntensity()), m_directionalLightComponent(directionalLightComponent)
    {
        m_direction = m_directionalLightComponent->getDirection();
        m_csm = std::make_unique<CascadedShadowMap>(this);
    }

    SkyLight::SkyLight(SkyLightComponent* skyLightComponent)
        : Light(skyLightComponent->getColor(), skyLightComponent->getIntensity())
    {

    }

    SkyLight::~SkyLight()
    {

    }

    void SkyLight::buildFromHDRI(Texture2D* HDRI)
    {
        // 1. convert 2D HDRI (equirectangular map) to cubemap
        u32 numMips = std::log2(cubemapCaptureResolution);
        GfxTextureCube::Spec spec(cubemapCaptureResolution, numMips, PF_RGB16F);
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
                [this, f, HDRI](ProgramPipeline* p) {
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
                    p->setTexture("srcHDRI", HDRI->getGfxResource());
                },
                gfxPipelineState
            );
        }
        glGenerateTextureMipmap(m_cubemap->getGpuResource());

        // 2. build light probes
    }

    void SkyLight::buildFromScene(Scene* scene)
    {
        // 1. take a cubemap scene capture

        // 2. build light probes
    }

}