#include "AssetManager.h"
#include "SkyLight.h"
#include "CyanRenderer.h"

namespace Cyan
{
    u32 SkyLight::numInstances = 0u;

    SkyLight::SkyLight(Scene* scene, const glm::vec4& colorAndIntensity, const char* srcHDRI) {
        if (srcHDRI)
        {
            ITextureRenderable::Spec hdriSpec = { };
            std::string filename(srcHDRI);
            u32 found = filename.find_last_of('.');
            std::string imageName = filename.substr(found - 1, found + 1);
            srcEquirectTexture = AssetManager::importTexture2D(imageName.c_str(), srcHDRI, hdriSpec);

            ITextureRenderable::Spec cubemapSpec = { };
            cubemapSpec.type = TEX_CUBE;
            cubemapSpec.width = 1024u;
            cubemapSpec.height = 1024u;
            cubemapSpec.pixelFormat = PF_RGB16F;
            cubemapSpec.numMips = max(log2(cubemapSpec.width), 1);
            ITextureRenderable::Parameter cubemapParams = { };
            cubemapParams.minificationFilter = FM_BILINEAR;
            cubemapParams.magnificationFilter = FM_BILINEAR;
            cubemapParams.wrap_r = WM_CLAMP;
            cubemapParams.wrap_s = WM_CLAMP;
            cubemapParams.wrap_t = WM_CLAMP;

            char cubemapName[128] = { };
            sprintf(cubemapName, "SkyLightCubemap%u", numInstances);
            srcCubemap = AssetManager::createTextureCube(cubemapName, cubemapSpec, cubemapParams);

            irradianceProbe = std::unique_ptr<IrradianceProbe>(scene->createIrradianceProbe(srcCubemap, glm::uvec2(64)));
            reflectionProbe = std::unique_ptr<ReflectionProbe>(scene->createReflectionProbe(srcCubemap));
        }
        else
        {

        }

        numInstances++;
    }

    void SkyLight::build() {
        buildCubemap(srcEquirectTexture, srcCubemap);
        irradianceProbe->buildFromCubemap();
        reflectionProbe->buildFromCubemap();
    }

    void SkyLight::buildCubemap(Texture2DRenderable* srcEquirectMap, TextureCubeRenderable* dstCubemap) {
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(dstCubemap->resolution, dstCubemap->resolution));
        renderTarget->setColorBuffer(dstCubemap, 0u);

        Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "RenderToCubemapShader", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl" });
        Mesh* cubeMesh = AssetManager::getAsset<Mesh>("UnitCubeMesh");

        for (i32 f = 0; f < 6u; f++)
        {
            renderTarget->setDrawBuffers({ f });
            renderTarget->clear({ { f } });

            GfxPipelineState pipelineState;
            pipelineState.depth = DepthControl::kDisable;
            Renderer::get()->submitMesh(
                renderTarget.get(),
                { 0, 0, renderTarget->width, renderTarget->height},
                pipelineState,
                cubeMesh,
                shader,
                [this, srcEquirectMap, f](RenderTarget* renderTarget, Shader* shader) {
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
                    shader->setTexture("srcImageTexture", srcEquirectMap);
                    shader->setUniform("projection", camera.projection());
                    shader->setUniform("view", camera.view());
                });
        }
    }
}