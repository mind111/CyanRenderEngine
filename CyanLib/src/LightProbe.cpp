#include <stbi/stb_image.h>

#include "LightProbe.h"
#include "Framebuffer.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"
#include "Shader.h"
#include "RenderPass.h"

namespace Cyan
{
#if 0
    GfxTexture2D* ReflectionProbe::s_BRDFLookupTexture = nullptr;
#endif

    const glm::vec3 cameraFacingDirections[] = {
        {1.f, 0.f, 0.f},   // Right
        {-1.f, 0.f, 0.f},  // Left
        {0.f, 1.f, 0.f},   // Up
        {0.f, -1.f, 0.f},  // Down
        {0.f, 0.f, 1.f},   // Front
        {0.f, 0.f, -1.f},  // Back
    }; 

    const glm::vec3 worldUps[] = {
        {0.f, -1.f, 0.f},   // Right
        {0.f, -1.f, 0.f},   // Left
        {0.f, 0.f, 1.f},    // Up
        {0.f, 0.f, -1.f},   // Down
        {0.f, -1.f, 0.f},   // Forward
        {0.f, -1.f, 0.f},   // Back
    };

    LightProbe::LightProbe(const glm::vec3& position, u32 resolution)
        : m_scene(nullptr), m_position(position), m_srcCubemap(nullptr), m_resolution(resolution)
    {

    }

    void LightProbe::setScene(Scene* scene)
    {
        m_scene = scene;
    }

    IrradianceProbe::IrradianceProbe(const glm::vec3& position, u32 resolution)
        : LightProbe(position, resolution)
    {
        GfxTextureCube::Spec spec(m_resolution, 1, PF_RGB16F);
        m_irradianceCubemap = std::unique_ptr<GfxTextureCube>(GfxTextureCube::create(spec));
    }

    void IrradianceProbe::buildFrom(GfxTextureCube* srcCubemap)
    {
        if (srcCubemap != nullptr)
        {
            m_srcCubemap = srcCubemap;

            GPU_DEBUG_SCOPE(convolveIrradianceCubemapMarker, "ConvovleIrraidanceCubemap");

            auto renderer = Renderer::get();
            CreateVS(vs, "ConvolveIrradianceCubemapVS", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl");
            CreatePS(ps, "ConvolveIrradianceCubemapPS", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl");
            CreatePixelPipeline(pipeline, "BuildIrradianceCubemap", vs, ps);
            auto cube = AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get();

            for (i32 f = 0; f < 6u; ++f)
            {
                GfxPipelineState gfxPipelineState;
                gfxPipelineState.depth = DepthControl::kDisable;

                renderer->drawStaticMesh(
                    getFramebufferSize(m_irradianceCubemap.get()),
                    [this, f](RenderPass& pass) {
                        pass.setRenderTarget(RenderTarget(m_irradianceCubemap.get(), f), 0);
                    },
                    { 0u, 0u, (u32)m_irradianceCubemap->resolution, (u32)m_irradianceCubemap->resolution },
                    cube,
                    pipeline,
                    [this, f](ProgramPipeline* p) {
                        PerspectiveCamera camera;
                        camera.m_position = m_position;
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
                        p->setUniform("numSamplesInTheta", (f32)kNumSamplesInTheta);
                        p->setUniform("numSamplesInPhi", (f32)kNumSamplesInPhi);
                        p->setTexture("srcCubemap", m_srcCubemap);
                    },
                    gfxPipelineState
                );
            }
        }
    }

#if 0
    ReflectionProbe::ReflectionProbe(GfxTextureCube* srcCubemapTexture)
        : LightProbe(srcCubemapTexture)
    {
        initialize();
    }

    ReflectionProbe::ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution)
        : LightProbe(scene, p, sceneCaptureResolution), m_convolvedReflectionTexture(nullptr)
    {
        initialize();
    }

    void ReflectionProbe::initialize()
    {
        // convolved radiance texture
        u32 numMips = log2(m_resolution.x) + 1;
        GfxTextureCube::Spec spec(m_resolution.x, numMips, PF_RGB16F);
        SamplerCube sampler;
        sampler.minFilter = FM_TRILINEAR;
        sampler.magFilter = FM_BILINEAR;
        m_convolvedReflectionTexture = std::unique_ptr<GfxTextureCube>(GfxTextureCube::create(spec, sampler));

        if (!s_convolveReflectionPipeline)
        {
            CreateVS(vs, "ConvolveReflectionVS", SHADER_SOURCE_PATH "convolve_specular_v.glsl");
            CreatePS(ps, "ConvolveReflectionPS", SHADER_SOURCE_PATH "convolve_specular_p.glsl");
            s_convolveReflectionPipeline = ShaderManager::createPixelPipeline("ConvolveReflection", vs, ps);
        }

        // generate shared BRDF lookup texture
        if (!s_BRDFLookupTexture)
        {
            s_BRDFLookupTexture = buildBRDFLookupTexture();
        }
    }

    // todo: fix this, 
    GfxTexture2D* ReflectionProbe::buildBRDFLookupTexture()
    {
        GfxTexture2D::Spec spec(512u, 512u, 1, PF_RGBA16F);
        Sampler2D sampler;
        GfxTexture2D* outTexture = GfxTexture2D::create(spec, sampler);

        auto renderer = Renderer::get();
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;
        CreateVS(vs, "IntegrateBRDFVS", SHADER_SOURCE_PATH "integrate_BRDF_v.glsl");
        CreatePS(ps, "IntegrateBRDFPS", SHADER_SOURCE_PATH "integrate_BRDF_p.glsl");
        CreatePixelPipeline(pipeline, "IntegrateBRDF", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outTexture),
            [outTexture](RenderPass& pass) {
                pass.setRenderTarget(outTexture, 0);
            },
            pipeline,
            [](ProgramPipeline* p) {

            });

        return outTexture;
    }

    void ReflectionProbe::convolve()
    {
        auto renderer = Renderer::get();
        u32 kNumMips = m_convolvedReflectionTexture->numMips;
        u32 mipWidth = sceneCapture->resolution; 
        u32 mipHeight = sceneCapture->resolution;

        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            {
                for (i32 f = 0; f < 6u; f++)
                {
                    GfxPipelineState gfxPipelineState;
                    gfxPipelineState.depth = DepthControl::kDisable;
                    renderer->drawStaticMesh(
                        glm::uvec2(mipWidth, mipHeight),
                        [this, f, mip](RenderPass& pass) {
                            pass.setRenderTarget(RenderTarget(m_convolvedReflectionTexture.get(), f, mip), 0);
                        },
                        { 0u, 0u, mipWidth, mipHeight }, 
                        AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get(),
                        s_convolveReflectionPipeline,
                        [this, f, mip, kNumMips](ProgramPipeline* p) {
                            // Update view matrix
#if 0
                            PerspectiveCamera camera(
                                glm::vec3(0.f),
                                LightProbeCameras::cameraFacingDirections[f],
                                LightProbeCameras::worldUps[f],
                                glm::uvec2(m_convolvedReflectionTexture->resolution, m_convolvedReflectionTexture->resolution),
                                VM_SCENE_COLOR,
                                90.f,
                                0.1f,
                                100.f
                            );

                            p->setUniform("projection", camera.projection());
                            p->setUniform("view", camera.view());
                            p->setUniform("roughness", mip * (1.f / (kNumMips - 1)));
                            p->setTexture("envmapSampler", sceneCapture);
#endif
                        },
                    gfxPipelineState);
                }
            }

            mipWidth /= 2u;
            mipHeight /= 2u;
        }
    }

    void ReflectionProbe::build()
    {
        captureScene();
        convolve();
    }

    void ReflectionProbe::buildFromCubemap()
    {
        convolve();
    }

    void ReflectionProbe::debugRender()
    {

    }
#endif
}