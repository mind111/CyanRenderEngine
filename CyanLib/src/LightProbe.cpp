#include <stbi/stb_image.h>

#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"

namespace Cyan
{
    Texture2DBindless* ReflectionProbe::s_BRDFLookupTexture = nullptr;
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

        // create render target
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(sceneCapture->resolution, sceneCapture->resolution));
        renderTarget->setColorBuffer(sceneCapture, 0u);
        Renderer::get()->renderSceneToLightProbe(scene, this, renderTarget.get());
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
        m_convolvedIrradianceTexture = std::make_unique<TextureCube>("IrradianceProbe", spec, sampler);
        m_convolvedIrradianceTexture->init();

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

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y));
        renderTarget->setColorBuffer(m_convolvedIrradianceTexture.get(), 0u);
        {
            for (i32 f = 0; f < 6u; ++f)
            {
                renderTarget->setDrawBuffers({ f });
                renderTarget->clear({ { f } });

                GfxPipelineConfig config;
                config.depth = DepthControl::kDisable;
                renderer->drawMesh(
                    renderTarget.get(),
                    { 0u, 0u, renderTarget->width, renderTarget->height }, 
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
                    config);
            }
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
        m_convolvedReflectionTexture = std::make_unique<TextureCube>("ReflectionProbe", spec, sampler);
        m_convolvedReflectionTexture->init();

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
    Texture2DBindless* ReflectionProbe::buildBRDFLookupTexture()
    {
        Texture2D::Spec spec(512u, 512u, 1, PF_RGBA16F);
        Sampler2D sampler;
        Texture2DBindless* outTexture = new Texture2DBindless("BRDFLUT", spec, sampler);
        outTexture->init();

        RenderTarget* renderTarget = createRenderTarget(outTexture->width, outTexture->height);
        renderTarget->setColorBuffer(outTexture, 0u);
        auto renderer = Renderer::get();
        GfxPipelineConfig pipelineState;
        pipelineState.depth = DepthControl::kDisable;
        CreateVS(vs, "IntegrateBRDFVS", SHADER_SOURCE_PATH "integrate_BRDF_v.glsl");
        CreatePS(ps, "IntegrateBRDFPS", SHADER_SOURCE_PATH "integrate_BRDF_p.glsl");
        CreatePixelPipeline(pipeline, "IntegrateBRDF", vs, ps);
        renderer->drawFullscreenQuad(
            renderTarget,
            pipeline,
            [](VertexShader* vs, PixelShader* ps) {

            });

        glDeleteFramebuffers(1, &renderTarget->fbo);
        delete renderTarget;
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
            auto renderTarget = createRenderTarget(mipWidth, mipHeight);
            renderTarget->setColorBuffer(m_convolvedReflectionTexture.get(), 0u, mip);
            {
                for (i32 f = 0; f < 6u; f++)
                {
                    renderTarget->setDrawBuffers({ f });
                    renderTarget->clear({ { f } });
                    GfxPipelineConfig config;
                    config.depth = DepthControl::kDisable;
                    renderer->drawMesh(
                        renderTarget,
                        { 0u, 0u, renderTarget->width, renderTarget->height }, 
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
                    config);
                }
            }
            glDeleteFramebuffers(1, &renderTarget->fbo);
            delete renderTarget;

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