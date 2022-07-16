#include "stb_image.h"

#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"

namespace Cyan
{
    Mesh* unitCubeMesh = nullptr;
    Shader* s_debugRenderShader = nullptr;
    Texture2DRenderable* ReflectionProbe::s_BRDFLookupTexture = nullptr;
    Shader* IrradianceProbe::s_convolveIrradianceShader = nullptr;
    Shader* ReflectionProbe::s_convolveReflectionShader = nullptr;
    RegularBuffer* IrradianceProbe::m_rayBuffers = nullptr;

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

    Shader* getRenderProbeShader()
    {
        if (!s_debugRenderShader)
        {
            s_debugRenderShader = ShaderManager::createShader({ ShaderType::kVsPs, "RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs" });
        }
        return s_debugRenderShader;
    }

    LightProbe::LightProbe(TextureCubeRenderable* srcCubemapTexture)
        : scene(nullptr), position(glm::vec3(0.f)), resolution(glm::uvec2(srcCubemapTexture->resolution, srcCubemapTexture->resolution)), 
        debugSphereMesh(nullptr), sceneCapture(srcCubemapTexture)
    {

    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : scene(scene), position(p), resolution(resolution), debugSphereMesh(nullptr), sceneCapture(nullptr)
    {
        // debugSphereMesh = AssetManager::getAsset<Mesh>("sphere_mesh")->createInstance(scene);

        initialize();
    }

    void LightProbe::initialize()
    {
        ITextureRenderable::Spec spec = { };
        spec.width = resolution.x;
        spec.type = ITextureRenderable::Spec::Type::kTexCube;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
        // sceneCapture = AssetManager::createTextureCube("SceneCapture", spec);

        // shared cube mesh
        if (!unitCubeMesh)
        {
            auto assetManager = AssetManager::get();
            unitCubeMesh = assetManager->getAsset<Mesh>("UnitCubeMesh");
            if (!unitCubeMesh)
            {
                u32 numVertices = sizeof(cubeVertices) / sizeof(f32);
                std::vector<ISubmesh*> submeshes;
                std::vector<Triangles::Vertex> vertices(numVertices);
                std::vector<u32> indices(numVertices);
                for (u32 v = 0; v < numVertices; ++v)
                {
                    vertices[v].pos = glm::vec3(cubeVertices[v * 3 + 0], cubeVertices[v * 3 + 1], cubeVertices[v * 3 + 2]);
                    indices[v] = v;
                }
                submeshes.push_back(assetManager->createSubmesh<Triangles>(vertices, indices));
            }
        }
    }

    void LightProbe::captureScene()
    {
        CYAN_ASSERT(scene, "Attempt to call LightProbe::captureScene() while scene is NULL!");

        // create render target
        auto renderTarget = createRenderTarget(sceneCapture->resolution, sceneCapture->resolution);
        {
            renderTarget->setColorBuffer(sceneCapture, 0u);
            Renderer::get()->renderSceneToLightProbe(scene, this, renderTarget);
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->fbo);
        delete renderTarget;
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
        spec.type = ITextureRenderable::Spec::Type::kTexCube;
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
        // m_convolvedIrradianceTexture = AssetManager::createTextureCube("IrradianceProbe", spec);

        if (!s_convolveIrradianceShader)
        {
            s_convolveIrradianceShader = ShaderManager::createShader({ ShaderType::kVsPs, "ConvolveIrradianceShader", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl" });
        }
    }

    void IrradianceProbe::convolve()
    {
        Toolkit::GpuTimer timer("ConvolveIrradianceTimer");
        auto renderer = Renderer::get();

        auto renderTarget = createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y);
        renderTarget->setColorBuffer(m_convolvedIrradianceTexture, 0u);
        {
            for (u32 f = 0; f < 6u; ++f)
            {
                GfxPipelineState pipelineState;
                pipelineState.depth = DepthControl::kDisable;

                renderer->submitMesh(
                    renderTarget,
                    { { (i32)f } },
                    false,
                    { 0u, 0u, renderTarget->width, renderTarget->height }, 
                    pipelineState, 
                    unitCubeMesh,
                    s_convolveIrradianceShader,
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
                        shader->setUniform("view", camera.view())
                            .setUniform("projection", camera.projection())
                            .setTexture("srcCubemapTexture", sceneCapture);
                    });
            }
            timer.end();
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->fbo);
        delete renderTarget;
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
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = ITextureRenderable::Parameter::Filtering::MIPMAP_LINEAR;
        // m_convolvedReflectionTexture = AssetManager::createTextureCube("IrradianceProbe", spec, parameter);

        if (!s_convolveReflectionShader)
        {
            s_convolveReflectionShader = ShaderManager::createShader({ ShaderType::kVsPs, "ConvolveReflectionShader", SHADER_SOURCE_PATH "convolve_specular_v.glsl", SHADER_SOURCE_PATH "convolve_specular_p.glsl" });
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
        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16A16;
        Texture2DRenderable* outTexture = AssetManager::createTexture2D("BRDFLUT", spec);

        Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "IntegrateBRDFShader", SHADER_SOURCE_PATH "shader_integrate_brdf.vs", SHADER_SOURCE_PATH "shader_integrate_brdf.fs" });
        RenderTarget* renderTarget = createRenderTarget(outTexture->width, outTexture->height);
        renderTarget->setColorBuffer(outTexture, 0u);

        auto renderer = Renderer::get();
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;
        renderer->submitFullScreenPass(
            renderTarget,
            { { 0u } },
            shader,
            [](RenderTarget* renderTarget, Shader* shader) {

            });

        glDeleteFramebuffers(1, &renderTarget->fbo);
        delete renderTarget;
        return outTexture;
    }

    void ReflectionProbe::convolve()
    {
        auto renderer = Renderer::get();
        u32 kNumMips = 11;
        u32 mipWidth = sceneCapture->resolution; 
        u32 mipHeight = sceneCapture->resolution;

        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            auto renderTarget = createRenderTarget(mipWidth, mipHeight);
            renderTarget->setColorBuffer(m_convolvedReflectionTexture, 0u, mip);
            {
                for (u32 f = 0; f < 6u; f++)
                {
                    GfxPipelineState pipelineState;
                    pipelineState.depth = DepthControl::kDisable;
                    renderer->submitMesh(
                        renderTarget,
                        { { (i32)f } },
                        false,
                        { 0u, 0u, renderTarget->width, renderTarget->height }, 
                        pipelineState, 
                        unitCubeMesh, 
                        s_convolveReflectionShader,
                        [this, f, mip, kNumMips](RenderTarget* renderTarget, Shader* shader) {
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

                            shader->setUniform("projection", camera.projection())
                                    .setUniform("view", camera.view())
                                    .setUniform("roughness", mip * (1.f / (kNumMips - 1)))
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