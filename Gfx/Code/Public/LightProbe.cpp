#include "stb_image.h"

#include "LightProbe.h"
#include "GHFramebuffer.h"
#include "AssetManager.h"
#include "Shader.h"
#include "RenderPass.h"

namespace Cyan
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
        SamplerCube sampler = { };
        sampler.minFilter = FM_BILINEAR;
        sampler.magFilter = FM_BILINEAR;
        sampler.wrapS = WM_CLAMP;
        sampler.wrapT = WM_CLAMP;
        m_irradianceCubemap = std::unique_ptr<GfxTextureCube>(GfxTextureCube::create(spec, sampler));
    }

    void IrradianceProbe::buildFrom(GfxTextureCube* srcCubemap)
    {
        if (srcCubemap != nullptr)
        {
            m_srcCubemap = srcCubemap;

            GPU_DEBUG_SCOPE(buildIrradianceCubemap, "BuildIrradianceCubemap");

            auto renderer = Renderer::get();
            CreateVS(vs, "BuildCubemapVS", SHADER_SOURCE_PATH "build_cubemap_v.glsl");
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
                        Camera camera;
                        camera.m_worldSpacePosition = m_position;
                        camera.m_worldUp = worldUps[f];
                        camera.m_worldSpaceForward = cameraFacingDirections[f];
                        camera.m_perspective.n = .1f;
                        camera.m_perspective.f = 100.f;
                        camera.m_perspective.fov = 90.f;
                        camera.m_perspective.aspectRatio = 1.f;
                        p->setUniform("cameraView", camera.viewMatrix());
                        p->setUniform("cameraProjection", camera.projectionMatrix());
                        p->setUniform("numSamplesInTheta", (f32)kNumSamplesInTheta);
                        p->setUniform("numSamplesInPhi", (f32)kNumSamplesInPhi);
                        p->setTexture("srcCubemap", m_srcCubemap);
                    },
                    gfxPipelineState
                );
            }
        }
    }

    std::unique_ptr<GfxTexture2D> ReflectionProbe::s_BRDFLookupTexture = nullptr;
    ReflectionProbe::ReflectionProbe(const glm::vec3& position, u32 resolution)
        : LightProbe(position, resolution)
    {
        u32 numMips = log2(m_resolution) + 1;
        GfxTextureCube::Spec spec(m_resolution, numMips, PF_RGB16F);
        SamplerCube sampler;
        sampler.minFilter = FM_TRILINEAR;
        sampler.magFilter = FM_BILINEAR;
        sampler.wrapS = WM_CLAMP;
        sampler.wrapT = WM_CLAMP;
        m_reflectionCubemap.reset(GfxTextureCube::create(spec, sampler));

        if (s_BRDFLookupTexture == nullptr)
        {
            const glm::uvec2 BRDFLutResolution(512u);
            GfxTexture2D::Spec spec(BRDFLutResolution.x, BRDFLutResolution.y, 1, PF_RGBA16F);
            s_BRDFLookupTexture.reset(GfxTexture2D::create(spec));

            buildBRDFLookupTexture();
        }
    }

    void ReflectionProbe::buildFrom(GfxTextureCube* srcCubemap)
    {
        if (srcCubemap != nullptr)
        {
            m_srcCubemap = srcCubemap;

            GPU_DEBUG_SCOPE(convolveReflectionCubemap, "BuildReflectionCubemap");

            CreateVS(vs, "BuildCubemapVS", SHADER_SOURCE_PATH "build_cubemap_v.glsl");
            CreatePS(ps, "ConvolveReflectionPS", SHADER_SOURCE_PATH "convolve_specular_p.glsl");
            CreatePixelPipeline(p, "ConvolveReflection", vs, ps);

            auto renderer = Renderer::get();

            u32 kNumMips = m_reflectionCubemap->numMips;
            u32 mipWidth = m_reflectionCubemap->resolution;
            u32 mipHeight = m_reflectionCubemap->resolution;
            for (u32 mip = 0; mip < kNumMips; ++mip)
            {
                std::string markerName("Mip[");
                markerName += std::to_string(mip) + ']';
                markerName += '_';
                markerName += std::to_string(mipWidth) + 'x' + std::to_string(mipHeight);
                GPU_DEBUG_SCOPE(convolveReflectionOneMip, markerName.c_str());

                for (i32 f = 0; f < 6u; f++)
                {
                    GfxPipelineState gfxPipelineState;
                    gfxPipelineState.depth = DepthControl::kDisable;
                    renderer->drawStaticMesh(
                        glm::uvec2(mipWidth, mipHeight),
                        [this, f, mip](RenderPass& pass) {
                            pass.setRenderTarget(RenderTarget(m_reflectionCubemap.get(), f, mip), 0);
                        },
                        { 0u, 0u, mipWidth, mipHeight },
                        AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get(),
                        p,
                        [this, f, mip, kNumMips](ProgramPipeline* p) {
                            Camera camera;
                            camera.m_worldSpacePosition = m_position;
                            camera.m_worldUp = worldUps[f];
                            camera.m_worldSpaceForward = cameraFacingDirections[f];
                            camera.m_perspective.n = .1f;
                            camera.m_perspective.f = 100.f;
                            camera.m_perspective.fov = 90.f;
                            camera.m_perspective.aspectRatio = 1.f;
                            p->setUniform("cameraView", camera.viewMatrix());
                            p->setUniform("cameraProjection", camera.projectionMatrix());
                            p->setUniform("roughness", mip * (1.f / (kNumMips - 1)));
                            p->setTexture("srcCubemap", m_srcCubemap);
                        },
                        gfxPipelineState
                    );
                }
                mipWidth /= 2u;
                mipHeight /= 2u;
            }
        }
    }

    // todo: fix this !!!
    void ReflectionProbe::buildBRDFLookupTexture()
    {
        GPU_DEBUG_SCOPE(buildBRDFLut, "BuildBRDFLUT");

        auto renderer = Renderer::get();

        CreateVS(vs, "IntegrateBRDFVS", SHADER_SOURCE_PATH "integrate_BRDF_v.glsl");
        CreatePS(ps, "IntegrateBRDFPS", SHADER_SOURCE_PATH "integrate_BRDF_p.glsl");
        CreatePixelPipeline(pipeline, "IntegrateBRDF", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(s_BRDFLookupTexture.get()),
            [](RenderPass& pass) {
                pass.setRenderTarget(s_BRDFLookupTexture.get(), 0);
            },
            pipeline,
            [](ProgramPipeline* p) {

            }
        );
    }
}