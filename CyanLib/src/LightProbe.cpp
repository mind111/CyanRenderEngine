#include "stb_image.h"

#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"

namespace Cyan
{
    Texture2DRenderable* ReflectionProbe::s_BRDFLookupTexture = nullptr;
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

    LightProbe::LightProbe(TextureCubeRenderable* srcCubemapTexture)
        : scene(nullptr), position(glm::vec3(0.f)), resolution(glm::uvec2(srcCubemapTexture->resolution, srcCubemapTexture->resolution)), 
        debugSphereMesh(nullptr), sceneCapture(srcCubemapTexture)
    {
        initialize();
    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : scene(scene), position(p), resolution(resolution), debugSphereMesh(nullptr), sceneCapture(nullptr)
    {
        // debugSphereMesh = AssetManager::getAsset<Mesh>("sphere_mesh")->createInstance(scene);
        initialize();
    }

    void LightProbe::initialize() {
        ITextureRenderable::Spec spec = { };
        spec.width = resolution.x;
        spec.type = ITextureRenderable::Spec::Type::kTexCube;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
        // sceneCapture = AssetManager::createTextureCube("SceneCapture", spec);
    }

    void LightProbe::captureScene() {
        CYAN_ASSERT(scene, "Attempt to call LightProbe::captureScene() while scene is NULL!");
        assert(scene);

        // create render target
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(sceneCapture->resolution, sceneCapture->resolution));
        renderTarget->setColorBuffer(sceneCapture, 0u);
        Renderer::get()->renderSceneToLightProbe(scene, this, renderTarget.get());
    }

    void LightProbe::debugRender()
    {

    }

    IrradianceProbe::IrradianceProbe(TextureCubeRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes)
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
        ITextureRenderable::Spec spec = { };
        spec.width = m_irradianceTextureRes.x;
        spec.height = m_irradianceTextureRes.x;
        spec.depth = m_irradianceTextureRes.x;
        spec.type = ITextureRenderable::Spec::Type::kTexCube;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
        m_convolvedIrradianceTexture = AssetManager::createTextureCube("IrradianceProbe", spec);

        if (!s_convolveIrradiancePipeline)
        {
            CreateVS(vs, "ConvolveIrradianceVS", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl");
            CreatePS(ps, "ConvolveIrradiancePS", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl");
            s_convolveIrradiancePipeline = ShaderManager::createPixelPipeline("ConvolveIrradiance", vs, ps);
        }
    }

    void IrradianceProbe::convolve()
    {
        auto renderer = Renderer::get();

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y));
        renderTarget->setColorBuffer(m_convolvedIrradianceTexture, 0u);
        {
            for (i32 f = 0; f < 6u; ++f)
            {
                renderTarget->setDrawBuffers({ f });
                renderTarget->clear({ { f } });

                GfxPipelineState pipelineState;
                pipelineState.depth = DepthControl::kDisable;
                renderer->drawMesh(
                    renderTarget.get(),
                    { 0u, 0u, renderTarget->width, renderTarget->height }, 
                    pipelineState, 
                    AssetManager::getAsset<Mesh>("UnitCubeMesh"),
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
                    });
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

    ReflectionProbe::ReflectionProbe(TextureCubeRenderable* srcCubemapTexture)
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
        ITextureRenderable::Spec spec = { };
        spec.width = resolution.x;
        spec.type = ITextureRenderable::Spec::Type::kTexCube;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
        spec.numMips = max(1, log2(spec.width) + 1);
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::LINEAR_MIPMAP_LINEAR;
        m_convolvedReflectionTexture = AssetManager::createTextureCube("ReflectionProbe", spec, parameter);

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

    Texture2DRenderable* ReflectionProbe::buildBRDFLookupTexture()
    {
        ITextureRenderable::Spec spec = { };
        spec.width = 512u;
        spec.height = 512u;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA16F;
        spec.type = TEX_2D;
        ITextureRenderable::Parameter params = { };
        params.wrap_r = WM_CLAMP;
        params.wrap_s = WM_CLAMP;
        params.wrap_t = WM_CLAMP;
        Texture2DRenderable* outTexture = AssetManager::createTexture2D("BRDFLUT", spec, params);

        RenderTarget* renderTarget = createRenderTarget(outTexture->width, outTexture->height);
        renderTarget->setColorBuffer(outTexture, 0u);
        auto renderer = Renderer::get();
        GfxPipelineState pipelineState;
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
            renderTarget->setColorBuffer(m_convolvedReflectionTexture, 0u, mip);
            {
                for (i32 f = 0; f < 6u; f++)
                {
                    renderTarget->setDrawBuffers({ f });
                    renderTarget->clear({ { f } });
                    GfxPipelineState pipelineState;
                    pipelineState.depth = DepthControl::kDisable;
                    renderer->drawMesh(
                        renderTarget,
                        { 0u, 0u, renderTarget->width, renderTarget->height }, 
                        pipelineState, 
                        AssetManager::getAsset<Mesh>("UnitCubeMesh"),
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
                        });
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