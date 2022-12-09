#include "AssetManager.h"
#include "CyanRenderer.h"
#include "Camera.h"
#include "Lights.h"

namespace Cyan {
    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB)
    {
        // derive the 8 vertices of the aabb from pmin and pmax
        // d -- c
        // |    |
        // a -- b
        // f stands for front while b stands for back
        f32 width = worldSpaceAABB.pmax.x - worldSpaceAABB.pmin.x;
        f32 height = worldSpaceAABB.pmax.y - worldSpaceAABB.pmin.y;
        f32 depth = worldSpaceAABB.pmax.z - worldSpaceAABB.pmin.z;
        glm::vec3 fa = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, -height, 0.f);
        glm::vec3 fb = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(0.f, -height, 0.f);
        glm::vec3 fc = vec4ToVec3(worldSpaceAABB.pmax);
        glm::vec3 fd = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, 0.f, 0.f);

        glm::vec3 ba = fa + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bb = fb + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bc = fc + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bd = fd + glm::vec3(0.f, 0.f, -depth);

        glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -inLightDirection, glm::vec3(0.f, 1.f, 0.f));
        BoundingBox3D lightSpaceAABB = { };
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fa, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fd, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(ba, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bd, 1.f));
        return lightSpaceAABB;
    }

    void DirectionalLight::renderShadowMap(SceneRenderable& scene, Renderer* renderer) {
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(direction, scene.aabb);
        shadowMap->render(lightSpaceAABB, scene, renderer);
    }

    void CSMDirectionalLight::renderShadowMap(SceneRenderable& scene, Renderer* renderer) {
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(direction, scene.aabb);
        shadowMap->render(lightSpaceAABB, scene, renderer);
    }

    GpuCSMDirectionalLight CSMDirectionalLight::buildGpuLight() {
        GpuCSMDirectionalLight light = { };
        light.direction = glm::vec4(direction, 0.f);
        light.colorAndIntensity = colorAndIntensity;
        for (i32 i = 0; i < shadowMap->kNumCascades; ++i) {
            light.cascades[i].n = shadowMap->cascades[i].n;
            light.cascades[i].f = shadowMap->cascades[i].f;
            light.cascades[i].shadowMap.lightSpaceView = glm::lookAt(glm::vec3(0.f), -direction, glm::vec3(0.f, 1.f, 0.f));
            light.cascades[i].shadowMap.lightSpaceProjection = shadowMap->cascades[i].shadowMap->lightSpaceProjection;
            light.cascades[i].shadowMap.depthMapHandle = shadowMap->cascades[i].shadowMap->depthTexture->glHandle;
        }
        return light;
    }

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
            Renderer::get()->drawMesh(
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