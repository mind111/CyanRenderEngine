#include "AssetManager.h"
#include "AssetImporter.h"
#include "CyanRenderer.h"
#include "Camera.h"
#include "Lights.h"
#include "RenderPass.h"

namespace Cyan 
{
#if 0
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

        glm::mat4 lightSpaceView = glm::m_lookAt(glm::vec3(0.f), -inLightDirection, glm::vec3(0.f, 1.f, 0.f));
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

    void DirectionalLight::renderShadowMap(Scene* inScene, Renderer* renderer) 
    {
        shadowMap->render(inScene, renderer);
    }

    void DirectionalLight::setShaderParameters(PixelShader* ps)
    {
        p->setUniform("directionalLight.colorAndIntensity", colorAndIntensity);
        p->setUniform("directionalLight.direction", glm::vec4(direction, 0.f));
        shadowMap->setShaderParameters(ps);
    }

    u32 SkyLight::numInstances = 0u;

    SkyLight::SkyLight(Scene* scene, const glm::vec4& colorAndIntensity, const char* srcHDRI) 
    {
        if (srcHDRI)
        {
            std::string filename(srcHDRI);
            u32 found = filename.find_last_of('.');
            std::string imageName = filename.substr(found - 1, found + 1);
            {
                Sampler2D sampler;
                sampler.minFilter = FM_BILINEAR;
                sampler.magFilter = FM_BILINEAR;
                auto HDRIImage = AssetImporter::importImageSync(imageName.c_str(), srcHDRI);
                // srcEquirectTexture = AssetManager::importTexture2D(imageName.c_str(), srcHDRI, sampler);
                srcEquirectTexture = AssetManager::createTexture2D(imageName.c_str(), HDRIImage, sampler);
            }

            u32 numMips = max(log2(1024u), 1) + 1;
            GfxTextureCube::Spec cubemapSpec(1024u, numMips, PF_RGB16F);
            SamplerCube sampler;
            srcCubemap = std::unique_ptr<GfxTextureCube>(GfxTextureCube::create(cubemapSpec, sampler));

            irradianceProbe = std::make_unique<IrradianceProbe>(srcCubemap.get(), glm::uvec2(64));
            reflectionProbe = std::make_unique<ReflectionProbe>(srcCubemap.get());
        }
        else
        {

        }

        numInstances++;
    }

    void SkyLight::build() {
        buildCubemap(srcEquirectTexture, srcCubemap.get());
        irradianceProbe->buildFromCubemap();
        reflectionProbe->buildFromCubemap();
    }

    void SkyLight::buildCubemap(Texture2D* srcEquirectMap, GfxTextureCube* dstCubemap) 
    {
#if 0
        auto framebuffer = std::unique_ptr<Framebuffer>(Framebuffer::create(dstCubemap->resolution, dstCubemap->resolution));
        framebuffer->setColorBuffer(dstCubemap, 0u);
#endif
        VertexShader* vs = ShaderManager::createShader<VertexShader>("RenderToCubemapVS", SHADER_SOURCE_PATH "render_to_cubemap_v.glsl");
        PixelShader* ps = ShaderManager::createShader<PixelShader>("RenderToCubemapPS", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
        PixelPipeline* pipeline = ShaderManager::createPixelPipeline("RenderToCubemap", vs, ps);
        StaticMesh* cubeMesh = AssetManager::getAsset<StaticMesh>("UnitCubeMesh");

        for (i32 f = 0; f < 6u; f++) 
        {
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;
            Renderer::get()->drawStaticMesh(
                getFramebufferSize(dstCubemap),
                [dstCubemap, f](RenderPass& pass) {
                    pass.setRenderTarget(RenderTarget(dstCubemap, f), 0);
                },
                { 0, 0, dstCubemap->resolution, dstCubemap->resolution },
                cubeMesh,
                pipeline,
                [this, srcEquirectMap, f](ProgramPipeline* p) {
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
                    p->setUniform("projection", camera.projection());
                    p->setUniform("view", camera.view());
                    p->setTexture("srcImageTexture", srcEquirectMap->gfxTexture.get());
                },
                gfxPipelineState
           );
        }
    }
#endif
}