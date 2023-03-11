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
    GfxTexture2DBindless* ReflectionProbe::s_BRDFLookupTexture = nullptr;
    PixelPipeline* IrradianceProbe::s_convolveIrradiancePipeline = nullptr;
    PixelPipeline* ReflectionProbe::s_convolveReflectionPipeline = nullptr;

    namespace LightProbeCameras
    {
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
    }

    LightProbe::LightProbe(TextureCube* srcCubemapTexture)
        : scene(nullptr), position(glm::vec3(0.f)), resolution(glm::uvec2(srcCubemapTexture->resolution, srcCubemapTexture->resolution)), 
        debugSphereMesh(nullptr), sceneCapture(srcCubemapTexture)
    {
        init();
    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : scene(scene), position(p), resolution(resolution), debugSphereMesh(nullptr), sceneCapture(nullptr)
    {
        // debugSphereMesh = AssetManager::getAsset<Mesh>("sphere_mesh")->createInstance(scene);
        init();
    }

    void LightProbe::init() 
    {
        if (!sceneCapture)
        {
            assert(0);
        }
    }

    void LightProbe::captureScene() 
    {
        assert(scene);

// todo: implement this at some point
#if 0
        // create render target
        auto framebuffer = std::unique_ptr<Framebuffer>(createFramebuffer(sceneCapture->resolution, sceneCapture->resolution));
        framebuffer->setColorBuffer(sceneCapture, 0u);
        Renderer::get()->renderSceneToLightProbe(scene, this, framebuffer.get());
#endif
    }

    void LightProbe::debugRender()
    {

    }

    IrradianceProbe::IrradianceProbe(TextureCube* srcCubemapTexture, const glm::uvec2& irradianceRes)
        : LightProbe(srcCubemapTexture), m_irradianceTextureRes(irradianceRes)
    {
        initialize();
    }

    IrradianceProbe::IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceRes)
        : LightProbe(scene, p, sceneCaptureResolution), m_irradianceTextureRes(irradianceRes)
    {
        initialize();
    }

    void IrradianceProbe::initialize()
    {
        TextureCube::Spec spec(m_irradianceTextureRes.x, 1, PF_RGB16F);
        SamplerCube sampler;
        sampler.minFilter = FM_BILINEAR;
        sampler.magFilter = FM_BILINEAR;
        m_convolvedIrradianceTexture = std::unique_ptr<TextureCube>(TextureCube::create(spec, sampler));

        if (!s_convolveIrradiancePipeline)
        {
            CreateVS(vs, "ConvolveIrradianceVS", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl");
            CreatePS(ps, "ConvolveIrradiancePS", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl");
            s_convolveIrradiancePipeline = ShaderManager::createPixelPipeline("ConvolveIrradiance", vs, ps);
        }
    }

    void IrradianceProbe::convolve()
    {
        assert(sceneCapture);
        auto renderer = Renderer::get();

        for (i32 f = 0; f < 6u; ++f)
        {
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;

            renderer->drawStaticMesh(
                getFramebufferSize(m_convolvedIrradianceTexture.get()),
                [this, f](RenderPass& pass) {
                    pass.setRenderTarget(RenderTarget(m_convolvedIrradianceTexture.get(), f), 0);
                },
                { 0u, 0u, (u32)m_convolvedIrradianceTexture->resolution, (u32)m_convolvedIrradianceTexture->resolution }, 
                AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
                s_convolveIrradiancePipeline,
                [this, f](VertexShader* vs, PixelShader* ps) {
                    // Update view matrix
                    PerspectiveCamera camera(
                        glm::vec3(0.f),
                        LightProbeCameras::cameraFacingDirections[f],
                        LightProbeCameras::worldUps[f],
                        90.f,
                        0.1f,
                        100.f,
                        1.0f
                    );
                    vs->setUniform("view", camera.view())
                        .setUniform("projection", camera.projection());
                    ps->setTexture("srcCubemapTexture", sceneCapture);
                },
                gfxPipelineState
            );
        }
    }

    void IrradianceProbe::build()
    {
        captureScene();
        convolve();
    }

    void IrradianceProbe::buildFromCubemap()
    {
        convolve();
    }

    void IrradianceProbe::debugRender()
    {

    }

    ReflectionProbe::ReflectionProbe(TextureCube* srcCubemapTexture)
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
        u32 numMips = log2(resolution.x) + 1;
        TextureCube::Spec spec(resolution.x, numMips, PF_RGB16F);
        SamplerCube sampler;
        sampler.minFilter = FM_TRILINEAR;
        sampler.magFilter = FM_BILINEAR;
        m_convolvedReflectionTexture = std::unique_ptr<TextureCube>(TextureCube::create(spec, sampler));

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
    GfxTexture2DBindless* ReflectionProbe::buildBRDFLookupTexture()
    {
        GfxTexture2D::Spec spec(512u, 512u, 1, PF_RGBA16F);
        Sampler2D sampler;
        GfxTexture2DBindless* outTexture = GfxTexture2DBindless::create(spec, sampler);

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
            [](VertexShader* vs, PixelShader* ps) {

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
                        AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
                        s_convolveReflectionPipeline,
                        [this, f, mip, kNumMips](VertexShader* vs, PixelShader* ps) {
                            // Update view matrix
                            PerspectiveCamera camera(
                                glm::vec3(0.f),
                                LightProbeCameras::cameraFacingDirections[f],
                                LightProbeCameras::worldUps[f],
                                90.f,
                                0.1f,
                                100.f,
                                1.0f
                            );

                            vs->setUniform("projection", camera.projection())
                                .setUniform("view", camera.view());
                            ps->setUniform("roughness", mip * (1.f / (kNumMips - 1)))
                                .setTexture("envmapSampler", sceneCapture);
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
}