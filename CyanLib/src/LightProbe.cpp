#include "stb_image.h"

#include "LightProbe.h"
#include "RenderTarget.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"
#include "Asset.h"

namespace Cyan
{
    Mesh*          unitCubeMesh                                = nullptr;
    Shader*        s_debugRenderShader                         = nullptr;
    Texture*       ReflectionProbe::s_BRDFLookupTexture        = nullptr;
    Shader*        IrradianceProbe::s_convolveIrradianceShader = nullptr;
    Shader*        ReflectionProbe::s_convolveReflectionShader = nullptr;
    RegularBuffer* IrradianceProbe::m_rayBuffers               = nullptr;

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
            s_debugRenderShader = createShader("RenderProbeShader", "../../shader/shader_render_probe.vs", "../../shader/shader_render_probe.fs");
        }
        return s_debugRenderShader;
    }

    LightProbe::LightProbe(Texture* srcCubemapTexture)
        : scene(nullptr), position(glm::vec3(0.f)), resolution(glm::uvec2(srcCubemapTexture->width, srcCubemapTexture->height)), 
        debugSphereMesh(nullptr), sceneCapture(srcCubemapTexture), debugRenderMatl(nullptr)
    {

    }

    LightProbe::LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution)
        : scene(scene), position(p), resolution(resolution), debugSphereMesh(nullptr), sceneCapture(nullptr), debugRenderMatl(nullptr)
    {
        // debugSphereMesh = AssetManager::getAsset<Mesh>("sphere_mesh")->createInstance(scene);
        // debugRenderMatl = createMaterial(getRenderProbeShader())->createInstance();

        initialize();
    }

    void LightProbe::initialize()
    {
        TextureSpec spec = { };
        spec.width = resolution.x;
        spec.height = resolution.y;
        spec.type = Texture::Type::TEX_CUBEMAP;
        spec.format = Texture::ColorFormat::R16G16B16;
        spec.dataType =  Texture::Float;
        spec.min = Texture::Filter::LINEAR;
        spec.mag = Texture::Filter::LINEAR;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.numMips = 1u;
        spec.data = 0;
        auto textureManager = TextureManager::getSingletonPtr();
        sceneCapture = textureManager->createTextureHDR("SceneCapture", spec);

        // shared cube mesh
        if (!unitCubeMesh)
        {
            auto assetManager = AssetManager::get();
            unitCubeMesh = assetManager->getAsset<Mesh>("UnitCubeMesh");
            if (!unitCubeMesh)
            {
                u32 numVertices = sizeof(cubeVertices) / sizeof(f32);
                std::vector<BaseSubmesh*> submeshes;
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
        auto renderTarget = createRenderTarget(sceneCapture->width, sceneCapture->height);
        {
            renderTarget->setColorBuffer(sceneCapture, 0u);
            Renderer::getSingletonPtr()->renderSceneToLightProbe(scene, this, renderTarget);
        }
        // release resources
        glDeleteFramebuffers(1, &renderTarget->fbo);
        delete renderTarget;
    }

    void LightProbe::debugRender()
    {

    }

    IrradianceProbe::IrradianceProbe(Texture* srcCubemapTexture, const glm::uvec2& irradianceRes)
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
        TextureSpec spec = { };
        spec.width = m_irradianceTextureRes.x;
        spec.height = m_irradianceTextureRes.y;
        spec.type = Texture::Type::TEX_CUBEMAP;
        spec.format = Texture::ColorFormat::R16G16B16;
        spec.dataType =  Texture::Float;
        spec.min = Texture::Filter::LINEAR;
        spec.mag = Texture::Filter::LINEAR;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.numMips = 1u;
        spec.data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        auto sceneManager = SceneManager::getSingletonPtr();
        m_convolvedIrradianceTexture = textureManager->createTextureHDR("IrradianceProbe", spec);

        if (!s_convolveIrradianceShader)
        {
            s_convolveIrradianceShader = createShader("ConvolveIrradianceShader", SHADER_SOURCE_PATH "convolve_diffuse_v.glsl", SHADER_SOURCE_PATH "convolve_diffuse_p.glsl");
        }
        // m_convolveIrradianceMatl = createMaterial(s_convolveIrradianceShader)->createInstance();
    }

    void IrradianceProbe::convolve()
    {
        Toolkit::GpuTimer timer("ConvolveIrradianceTimer");
        auto ctx = Cyan::getCurrentGfxCtx();
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        auto renderTarget = createRenderTarget(m_irradianceTextureRes.x, m_irradianceTextureRes.y);
        renderTarget->setColorBuffer(m_convolvedIrradianceTexture, 0u);
        {
            ctx->setShader(s_convolveIrradianceShader);
            ctx->setViewport({0u, 0u, renderTarget->width, renderTarget->height});
            ctx->setDepthControl(DepthControl::kDisable);
            for (u32 f = 0; f < 6u; ++f)
            {
                ctx->setRenderTarget(renderTarget, { (i32)f });

                camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                camera.worldUp = LightProbeCameras::worldUps[f];
                camera.update();
#if 0
                m_convolveIrradianceMatl->set("view", &camera.view[0]);
                m_convolveIrradianceMatl->set("projection", &camera.projection[0]);
                m_convolveIrradianceMatl->bindTexture("srcCubemapTexture", sceneCapture);
                m_convolveIrradianceMatl->bindToShader();
#endif
                s_convolveIrradianceShader->setUniformMat4("view", &camera.view[0].x)
                                        .setUniformMat4("projection", &camera.projection[0].x)
                                        .setTexture("srcCubemapTexture", sceneCapture);

                ctx->drawIndexAuto(36u);
            }
            ctx->setDepthControl(DepthControl::kEnable);
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

    ReflectionProbe::ReflectionProbe(Texture* srcCubemapTexture)
        : LightProbe(srcCubemapTexture)
    {
        initialize();
    }

    ReflectionProbe::ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution)
        : LightProbe(scene, p, sceneCaptureResolution), m_convolvedReflectionTexture(nullptr), m_convolveReflectionMatl(nullptr)
    {
        initialize();
    }

    void ReflectionProbe::initialize()
    {
        // convolved radiance texture
        TextureSpec spec = { };
        spec.width = resolution.x;
        spec.height = resolution.y;
        spec.type = Texture::Type::TEX_CUBEMAP;
        spec.format = Texture::ColorFormat::R16G16B16;
        spec.dataType = Texture::Float;
        spec.min = Texture::Filter::MIPMAP_LINEAR;
        spec.mag = Texture::Filter::LINEAR;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.numMips = kNumMips;
        spec.data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        m_convolvedReflectionTexture = textureManager->createTextureHDR("ConvolvedReflectionProbe", spec);

        if (!s_convolveReflectionShader)
        {
            s_convolveReflectionShader = createShader("ConvolveReflectionShader", SHADER_SOURCE_PATH "convolve_specular_v.glsl", SHADER_SOURCE_PATH "convolve_specular_p.glsl");
        }
        // m_convolveReflectionMatl = createMaterial(s_convolveReflectionShader)->createInstance();

        // generate shared BRDF lookup texture
        if (!s_BRDFLookupTexture)
        {
            s_BRDFLookupTexture = buildBRDFLookupTexture();
        }
    }

    Texture* ReflectionProbe::buildBRDFLookupTexture()
    {
        const u32 kTexWidth = 512u;
        const u32 kTexHeight = 512u;
        TextureSpec spec = { };
        spec.type = Texture::Type::TEX_2D;
        spec.format = Texture::ColorFormat::R16G16B16A16;
        spec.dataType = Texture::DataType::Float;
        spec.numMips = 1u;
        spec.width = kTexWidth;
        spec.height = kTexHeight;
        spec.min = Texture::Filter::LINEAR;
        spec.mag = Texture::Filter::LINEAR;
        spec.s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.r = Texture::Wrap::CLAMP_TO_EDGE;
        spec.data = nullptr;
        auto textureManager = TextureManager::getSingletonPtr();
        Texture* outTexture = textureManager->createTextureHDR("BRDFLUT", spec); 
        Shader* shader = createShader("IntegrateBRDFShader", "../../shader/shader_integrate_brdf.vs", "../../shader/shader_integrate_brdf.fs");
        RenderTarget* rt = createRenderTarget(kTexWidth, kTexWidth);
        rt->setColorBuffer(outTexture, 0u);
        f32 verts[] = {
            -1.f,  1.f, 0.f, 0.f,  1.f,
            -1.f, -1.f, 0.f, 0.f,  0.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
            -1.f,  1.f, 0.f, 0.f,  1.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
             1.f,  1.f, 0.f, 1.f,  1.f
        };
        GLuint vbo, vao;
        glCreateBuffers(1, &vbo);
        glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glNamedBufferData(vbo, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexArrayAttrib(vao, 0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
        glEnableVertexArrayAttrib(vao, 1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        auto ctx = getCurrentGfxCtx();
        ctx->setRenderTarget(rt, { 0 });
        ctx->setViewport({ 0, 0, kTexWidth, kTexHeight } );
        ctx->setShader(shader);
        glBindVertexArray(vao);
        ctx->setDepthControl(Cyan::DepthControl::kDisable);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        ctx->setDepthControl(Cyan::DepthControl::kEnable);
        ctx->reset();
        glDeleteFramebuffers(1, &rt->fbo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        return outTexture;
    }

    void ReflectionProbe::convolve()
    {
        Camera camera = { };
        // camera set to probe's location
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        camera.n = 0.1f;
        camera.f = 100.f;
        camera.fov = glm::radians(90.f);
        auto ctx = getCurrentGfxCtx();
        u32 kNumMips = 11;
        u32 mipWidth = sceneCapture->width; 
        u32 mipHeight = sceneCapture->height;
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setShader(s_convolveReflectionShader);
        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            auto renderTarget = createRenderTarget(mipWidth, mipHeight);
            renderTarget->setColorBuffer(m_convolvedReflectionTexture, 0u, mip);
            ctx->setViewport({ 0u, 0u, renderTarget->width, renderTarget->height });
            {
                for (u32 f = 0; f < 6u; f++)
                {
                    ctx->setRenderTarget(renderTarget, { (i32)f });

                    camera.lookAt = LightProbeCameras::cameraFacingDirections[f];
                    camera.worldUp = LightProbeCameras::worldUps[f];
                    camera.update();

                    s_convolveReflectionShader->setUniformMat4("projection", &camera.projection[0].x)
                                            .setUniformMat4("view", &camera.view[0].x)
                                            .setUniform1f("roughness", mip * (1.f / (kNumMips - 1)))
                                            .setTexture("envmapSampler", sceneCapture);
                    /*
                    m_convolveReflectionMatl->set("view", &camera.view[0]);
                    m_convolveReflectionMatl->set("roughness", mip * (1.f / (kNumMips - 1)));
                    m_convolveReflectionMatl->bindTexture("envmapSampler", sceneCapture);
                    m_convolveReflectionMatl->bindToShader();
                    */
                    ctx->drawIndexAuto(36);
                }
            }
            glDeleteFramebuffers(1, &renderTarget->fbo);
            delete renderTarget;

            mipWidth /= 2u;
            mipHeight /= 2u;
        }
        ctx->setDepthControl(DepthControl::kEnable);
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