#include "Lights.h"
#include "Shader.h"
#include "GfxContext.h"
#include "RenderPass.h"
#include "RenderingUtils.h"

namespace Cyan 
{
    Light::Light(const glm::vec3& color, f32 intensity)
        : m_color(color), m_intensity(intensity)
    {

    }

    Light::~Light()
    {

    }

    DirectionalLight::DirectionalLight(const glm::vec3& color, f32 intensity, const glm::vec3& direction)
        : Light(color, intensity), m_direction(direction)
    {
    }

    SkyLight::SkyLight(i32 irradianceResolution, i32 reflectionResolution)
    {
        // todo: hardcode the cubemap resolution for now
        {
            const i32 cubemapRes = 1024;
            const auto& desc = GHTextureCube::Desc::create(cubemapRes, 1);
            GHSamplerCube samplerCube;
            samplerCube.setFilteringModeMin(Sampler2DFilteringMode::Bilinear);
            samplerCube.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
            m_cubemap = std::move(GHTextureCube::create(desc, samplerCube));
        }

        m_irradianceCubemap = std::make_unique<IrradianceCubemap>(irradianceResolution);
        m_reflectionCubemap = std::make_unique<ReflectionCubemap>(reflectionResolution);
    }

    SkyLight::SkyLight()
        : SkyLight(64, 1024)
    {
    }

    SkyLight::~SkyLight()
    {
    }

    void SkyLight::buildFromHDRI(GHTexture2D* HDRITex)
    {
        // build a cubemap from HDRI first
        RenderingUtils::buildCubemapFromHDRI(m_cubemap.get(), HDRITex);
        buildFromCubemap(m_cubemap.get());
    }

    void SkyLight::buildFromCubemap(GHTextureCube* cubemap)
    {
        m_irradianceCubemap->buildFromCubemap(cubemap);
        m_reflectionCubemap->buildFromCubemap(cubemap);
    }

    IrradianceCubemap::IrradianceCubemap(i32 inResolution)
        : resolution(inResolution)
    {
        const auto& desc = GHTextureCube::Desc::create(resolution, 1);
        GHSamplerCube samplerCube;
        samplerCube.setAddressingModeX(SamplerAddressingMode::Clamp);
        samplerCube.setAddressingModeY(SamplerAddressingMode::Clamp);
        samplerCube.setFilteringModeMin(Sampler2DFilteringMode::Bilinear);
        samplerCube.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
        cubemap = std::move(GHTextureCube::create(desc, samplerCube));
    }

    static const glm::vec3 s_cameraFacingDirections[] = {
        { 1.f, 0.f, 0.f },   // Right
        {-1.f, 0.f, 0.f },  // Left
        { 0.f, 1.f, 0.f },   // Up
        { 0.f,-1.f, 0.f },  // Down
        { 0.f, 0.f, 1.f },   // Front
        { 0.f, 0.f,-1.f },  // Back
    };

    static const glm::vec3 s_worldUps[] = {
        { 0.f, -1.f, 0.f },   // Right
        { 0.f, -1.f, 0.f },   // Left
        { 0.f,  0.f, 1.f },    // Up
        { 0.f,  0.f,-1.f },   // Down
        { 0.f, -1.f, 0.f },   // Forward
        { 0.f, -1.f, 0.f },   // Back
    };

    void IrradianceCubemap::buildFromCubemap(GHTextureCube* srcCubemap)
    {
        GPU_DEBUG_SCOPE(marker, "Build Irradiance Cubemap")

        if (srcCubemap != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findShader<VertexShader>("BuildSkyboxVS");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BuildIrradianceCubemapPS", ENGINE_SHADER_PATH "build_irradiance_cubemap_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
            auto cubeMesh = RenderingUtils::get()->getUnitCubeMesh();

            const i32 kNumCubeFaces = 6u;
            for (i32 f = 0; f < kNumCubeFaces; f++)
            {
                const auto& cubemapDesc = cubemap->getDesc();
                RenderPass pass(cubemapDesc.resolution, cubemapDesc.resolution);
                RenderTarget rt(cubemap.get(), f, 0);
                pass.setRenderTarget(rt, 0);
                pass.setRenderFunc([gfxp, srcCubemap, f, cubeMesh](GfxContext* p) {

                    Transform cameraTransform = { };
                    cameraTransform.translation = glm::vec3(0.f);
                    CameraViewInfo cameraInfo(cameraTransform);
                    cameraInfo.m_worldUp = s_worldUps[f];
                    cameraInfo.m_worldSpaceForward = s_cameraFacingDirections[f];
                    cameraInfo.m_perspective.n = .1f;
                    cameraInfo.m_perspective.f = 100.f;
                    cameraInfo.m_perspective.fov = 90.f;
                    cameraInfo.m_perspective.aspectRatio = 1.f;

                    gfxp->bind();
                    gfxp->setUniform("u_viewMatrix", cameraInfo.viewMatrix());
                    gfxp->setUniform("u_projectionMatrix", cameraInfo.projectionMatrix());
                    gfxp->setUniform("u_numSamplesInTheta", 32.f);
                    gfxp->setUniform("u_numSamplesInPhi", 64.f);
                    gfxp->setTexture("u_srcCubemap", srcCubemap);

                    cubeMesh->draw();

                    gfxp->unbind();
                });
                pass.disableDepthTest();
                pass.render(GfxContext::get());
            }
        }
    }

    std::unique_ptr<GHTexture2D> ReflectionCubemap::BRDFLUT = nullptr;

    void ReflectionCubemap::buildBRDFLUT()
    {
        GPU_DEBUG_SCOPE(marker, "Build Reflection BRDF LUT")

        const auto& desc = GHTexture2D::Desc::create(512, 512, 1, PF_RGBA16F);
        GHSampler2D sampler = { };
        sampler.setAddressingModeX(SamplerAddressingMode::Clamp);
        sampler.setAddressingModeY(SamplerAddressingMode::Clamp);
        sampler.setFilteringModeMin(Sampler2DFilteringMode::Bilinear);
        sampler.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
        BRDFLUT = std::move(GHTexture2D::create(desc, sampler));

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "BuildBRDFLUTVS", ENGINE_SHADER_PATH "integrate_BRDF_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BuildBRDFLUTPS", ENGINE_SHADER_PATH "integrate_BRDF_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        RenderingUtils::renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [](RenderPass& pass) {
                RenderTarget rt(BRDFLUT.get(), 0);
                pass.setRenderTarget(rt, 0);
            },
            gfxp.get(),
            [](GfxPipeline* p) {

            }
        );
    }

    ReflectionCubemap::ReflectionCubemap(i32 inResolution)
        : resolution(inResolution)
    {
        assert(isPowerOf2(resolution));
        u32 numMips = glm::log2(resolution) + 1;
        const auto& desc = GHTextureCube::Desc::create(resolution, numMips);
        GHSamplerCube samplerCube;
        samplerCube.setAddressingModeX(SamplerAddressingMode::Clamp);
        samplerCube.setAddressingModeY(SamplerAddressingMode::Clamp);
        samplerCube.setFilteringModeMin(Sampler2DFilteringMode::Trilinear);
        samplerCube.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
        cubemap = std::move(GHTextureCube::create(desc, samplerCube));
    }

    void ReflectionCubemap::buildFromCubemap(GHTextureCube* srcCubemap)
    {
        // todo: build BRDF LUT
        if (BRDFLUT == nullptr)
        {
            buildBRDFLUT();
        }

        GPU_DEBUG_SCOPE(marker, "Build Reflection Cubemap")

        if (srcCubemap != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findShader<VertexShader>("BuildSkyboxVS");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BuildReflectionCubemapPS", ENGINE_SHADER_PATH "build_reflection_cubemap_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
            auto cubeMesh = RenderingUtils::get()->getUnitCubeMesh();
            const auto& cubemapDesc = cubemap->getDesc();
            i32 numMips = cubemapDesc.numMips;

            for (i32 mipLevel = 0; mipLevel < numMips; ++mipLevel)
            {
                i32 mipSize = 0;
                cubemap->getMipSize(mipSize, mipLevel);

                const i32 kNumCubeFaces = 6u;
                for (i32 f = 0; f < kNumCubeFaces; f++)
                {
                    RenderPass pass(mipSize, mipSize);
                    RenderTarget rt(cubemap.get(), f, mipLevel);
                    pass.setRenderTarget(rt, 0);
                    pass.setRenderFunc([gfxp, srcCubemap, f, cubeMesh, mipLevel, numMips](GfxContext* p) {

                        Transform cameraTransform = { };
                        cameraTransform.translation = glm::vec3(0.f);
                        CameraViewInfo cameraInfo(cameraTransform);
                        cameraInfo.m_worldUp = s_worldUps[f];
                        cameraInfo.m_worldSpaceForward = s_cameraFacingDirections[f];
                        cameraInfo.m_perspective.n = .1f;
                        cameraInfo.m_perspective.f = 100.f;
                        cameraInfo.m_perspective.fov = 90.f;
                        cameraInfo.m_perspective.aspectRatio = 1.f;

                        gfxp->bind();
                        gfxp->setUniform("u_viewMatrix", cameraInfo.viewMatrix());
                        gfxp->setUniform("u_projectionMatrix", cameraInfo.projectionMatrix());
                        gfxp->setUniform("u_roughness", (f32)mipLevel / (numMips - 1));
                        gfxp->setTexture("u_srcCubemap", srcCubemap);

                        cubeMesh->draw();

                        gfxp->unbind();
                    });
                    pass.disableDepthTest();
                    pass.render(GfxContext::get());
                }
            }
        }
    }
}