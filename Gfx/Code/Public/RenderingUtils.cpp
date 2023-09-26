#include "stbi/stb_image.h"

#include "RenderingUtils.h"
#include "RenderPass.h"
#include "GfxModule.h"
#include "GfxStaticMesh.h"
#include "Geometry.h"
#include "GfxHardwareAbstraction/GHInterface/GHBuffer.h"

namespace Cyan
{
    RenderingUtils* RenderingUtils::s_instance = nullptr;
    GfxStaticSubMesh* RenderingUtils::s_unitQuadMesh = nullptr;
    GfxStaticSubMesh* RenderingUtils::s_unitCubeMesh = nullptr;
    RenderingUtils::NoiseTextures RenderingUtils::s_noiseTextures = { };

    RenderingUtils::RenderingUtils()
    {
    }

    RenderingUtils::~RenderingUtils()
    {

    }

    RenderingUtils* RenderingUtils::get()
    {
        static std::unique_ptr<RenderingUtils> instance(new RenderingUtils());
        if (s_instance == nullptr)
        {
            s_instance = instance.get();
        }
        return s_instance;
    }

    void RenderingUtils::initialize()
    {
        // init unit quad
        {
            std::vector<Triangles::Vertex> vertices(6);
            vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
            vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
            vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
            vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
            std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
            std::unique_ptr<Geometry> unitQuad = std::make_unique<Triangles>(vertices, indices);
            s_unitQuadMesh = GfxStaticSubMesh::create(std::string("UnitQuad"), unitQuad.get());
        }

        // init unit cube
        {
            float positions[] = {
                -1.0f,  1.0f, -1.0f,
                -1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                -1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f
            };

            u32 numVertices = sizeof(positions) / sizeof(glm::vec3);
            std::vector<Triangles::Vertex> vertices(numVertices);
            std::vector<u32> indices(numVertices);
            for (u32 v = 0; v < numVertices; ++v)
            {
                vertices[v].pos = glm::vec3(positions[v * 3 + 0], positions[v * 3 + 1], positions[v * 3 + 2]);
                indices[v] = v;
            }
            std::unique_ptr<Geometry> unitCube = std::make_unique<Triangles>(vertices, indices);
            s_unitCubeMesh = GfxStaticSubMesh::create(std::string("UnitCube"), unitCube.get());
        }

        // init textures
        {
            struct ImageInfo
            {
                i32 width = -1;
                i32 height = -1;
                i32 numChannels = -1;
                i32 bitsPerChannel = -1;
                std::unique_ptr<u8> pixels = nullptr;
            };

            auto loadImage = [](ImageInfo& outInfo, const char* filename) {
                i32 hdr = stbi_is_hdr(filename);
                if (hdr)
                {
                    outInfo.bitsPerChannel = 32;
                    outInfo.pixels.reset((u8*)stbi_loadf(filename, &outInfo.width, &outInfo.height, &outInfo.numChannels, 0));
                }
                else
                {
                    i32 is16Bit = stbi_is_16_bit(filename);
                    if (is16Bit)
                    {
                        outInfo.bitsPerChannel = 16;
                        outInfo.pixels.reset((u8*)stbi_load_16(filename, &outInfo.width, &outInfo.height, &outInfo.numChannels, 0));
                    }
                    else
                    {
                        outInfo.bitsPerChannel = 8;
                        outInfo.pixels.reset((u8*)stbi_load(filename, &outInfo.width, &outInfo.height, &outInfo.numChannels, 0));
                    }
                }
            };

            auto translatePF = [](const ImageInfo& info) -> PixelFormat {
                PixelFormat pf = PixelFormat::kCount;
                switch (info.numChannels)
                {
                case 1:
                    switch (info.bitsPerChannel)
                    {
                    case 8: pf = PF_R8; break;
                    case 16: pf = PF_R16F; break;
                    case 32: pf = PF_R32F; break;
                    }
                    break;
                case 2:
                    break;
                case 3:
                    switch (info.bitsPerChannel)
                    {
                    case 8: pf = PF_RGB8; break;
                    case 16: pf = PF_RGB16F; break;
                    case 32: pf = PF_RGB32F; break;
                    }
                    break;
                case 4:
                    switch (info.bitsPerChannel)
                    {
                    case 8: pf = PF_RGBA8; break;
                    case 16: pf = PF_RGBA16F; break;
                    case 32: pf = PF_RGBA32F; break;
                    }
                    break;
                default:
                    assert(0);
                }
                return pf;
            };

            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_0_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_0 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_1_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_1 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_2_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_2 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_3_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_3 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_4_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_4 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_5_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_5 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_6_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_6 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_7_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_R_7 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise1024x1024_RGBA_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise1024x1024_RGBA = GHTexture2D::create(desc);
            }
        }
    }

    void RenderingUtils::deinitialize()
    {

    }

    void RenderingUtils::renderScreenPass(const glm::uvec2& renderResolution, const RenderTargetSetupFunc& renderTargetSetupFunc, GfxPipeline* p, ShaderSetupFunc& shaderSetupFunc, bool bDepthTest)
    {
        RenderPass rp(renderResolution.x, renderResolution.y);
        renderTargetSetupFunc(rp);
        rp.setRenderFunc([p, &shaderSetupFunc](GfxContext* ctx) {
            p->bind();
            shaderSetupFunc(p);
            s_unitQuadMesh->draw();
            p->unbind();
        });
        bDepthTest ? rp.enableDepthTest() : rp.disableDepthTest();
        rp.render(GfxContext::get());
    }

    // todo: should the format of src and dst texture match in order for the copying to be considered as valid ..?
    GFX_API void RenderingUtils::copyTexture(GHTexture2D* dstTexture, u32 dstMip, GHTexture2D* srcTexture, u32 srcMip)
    {
        GPU_DEBUG_SCOPE(marker, "CopyTexture")
        
        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BlitTexturePS", ENGINE_SHADER_PATH "blit_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        const auto& desc = dstTexture->getDesc();
        renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [dstTexture, dstMip](RenderPass& rp) {
                RenderTarget rt(dstTexture, dstMip);
                rp.setRenderTarget(rt, 0);
            },
            gfxp.get(),
            [srcTexture, srcMip](GfxPipeline* p) {
                p->setTexture("srcTexture", srcTexture);
                p->setUniform("mipLevel", (i32)srcMip);
            }
        );
    }

    GFX_API void RenderingUtils::copyDepthTexture(GHDepthTexture* dstTexture, GHDepthTexture* srcTexture)
    {
        GPU_DEBUG_SCOPE(marker, "CopyDepthTexture")

        const auto& srcDesc = srcTexture->getDesc();
        const auto& dstDesc = dstTexture->getDesc();

        assert(srcDesc.width == dstDesc.width && srcDesc.height == dstDesc.height);

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BlitDepthTexturePS", ENGINE_SHADER_PATH "blit_depth_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        const auto& desc = dstTexture->getDesc();
        renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [dstTexture](RenderPass& rp) {
                DepthTarget dt(dstTexture);
                rp.setDepthTarget(dt);
            },
            gfxp.get(),
            [srcTexture](GfxPipeline* p) {
                p->setTexture("srcDepthTexture", srcTexture);
            },
            true
        );
    }

    GFX_API void RenderingUtils::clearTexture2D(GHTexture2D* textureToClear, const glm::vec4& clearColor)
    {
        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ClearTexturePS", ENGINE_SHADER_PATH "clear_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
        const auto& desc = textureToClear->getDesc();
        renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [textureToClear](RenderPass& rp) {
                RenderTarget rt(textureToClear, 0, false);
                rp.setRenderTarget(rt, 0);
            },
            gfxp.get(),
            [clearColor](GfxPipeline* p) {
                p->setUniform("u_clearColor", clearColor);
            }
        );
    }

    void RenderingUtils::renderToBackBuffer(GHTexture2D* srcTexture)
    {
        GfxModule* gfxModule = GfxModule::get();
        glm::uvec2 backbufferSize = GfxModule::get()->getWindowSize();

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BlitTexturePS", ENGINE_SHADER_PATH "blit_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        RenderPass rp(backbufferSize.x, backbufferSize.y);
        rp.enableRenderToDefaultTarget();
        rp.setRenderFunc([gfxp, srcTexture](GfxContext* ctx) {
            gfxp->bind();
            gfxp->setUniform("mipLevel", 0);
            gfxp->setTexture("srcTexture", srcTexture);
            s_unitQuadMesh->draw();
            gfxp->unbind();
        });
        rp.disableDepthTest();
        rp.render(GfxContext::get());
    }

    void RenderingUtils::renderToBackBuffer(GHTexture2D* srcTexture, const Viewport& viewport)
    {
        GfxModule* gfxModule = GfxModule::get();
        glm::uvec2 backbufferSize = GfxModule::get()->getWindowSize();

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BlitTexturePS", ENGINE_SHADER_PATH "blit_texture_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        RenderPass rp(backbufferSize.x, backbufferSize.y);
        rp.enableRenderToDefaultTarget();
        rp.setRenderFunc([gfxp, srcTexture](GfxContext* ctx) {
            gfxp->bind();
            gfxp->setUniform("mipLevel", 0);
            gfxp->setTexture("srcTexture", srcTexture);
            s_unitQuadMesh->draw();
            gfxp->unbind();
        });
        rp.setViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        rp.disableDepthTest();
        rp.render(GfxContext::get());
    }

    GFX_API void RenderingUtils::dispatchComputePass(ComputePipeline* cp, std::function<void(ComputePipeline*)>&& passLambda, i32 threadGroupSizeX, i32 threadGroupSizeY, i32 threadGroupSizeZ)
    {
        cp->bind();
        passLambda(cp);
        GfxContext::get()->dispatchCompute(cp, threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
        cp->unbind();
    }

    static const glm::vec3 s_cameraFacingDirections[] = {
        {1.f, 0.f, 0.f},   // Right
        {-1.f, 0.f, 0.f},  // Left
        {0.f, 1.f, 0.f},   // Up
        {0.f, -1.f, 0.f},  // Down
        {0.f, 0.f, 1.f},   // Front
        {0.f, 0.f, -1.f},  // Back
    }; 

    static const glm::vec3 s_worldUps[] = {
        {0.f, -1.f, 0.f},   // Right
        {0.f, -1.f, 0.f},   // Left
        {0.f, 0.f, 1.f},    // Up
        {0.f, 0.f, -1.f},   // Down
        {0.f, -1.f, 0.f},   // Forward
        {0.f, -1.f, 0.f},   // Back
    };

    GFX_API void RenderingUtils::buildCubemapFromHDRI(GHTextureCube* outTexCube, GHTexture2D* HDRITex)
    {
        GPU_DEBUG_SCOPE(marker, "Build Cubemap From HDRI")

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "BuildSkyboxVS", ENGINE_SHADER_PATH "build_skybox_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "BuildSkyboxPS", ENGINE_SHADER_PATH "build_skybox_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
        auto cubeMesh = RenderingUtils::get()->getUnitCubeMesh();

        const i32 kNumCubeFaces = 6u;
        for (i32 f = 0; f < kNumCubeFaces; f++)
        {
            const auto& cubemapDesc = outTexCube->getDesc();
            RenderPass pass(cubemapDesc.resolution, cubemapDesc.resolution);
            RenderTarget rt(outTexCube, f, 0);
            pass.setRenderTarget(rt, 0);
            pass.setRenderFunc([gfxp, HDRITex, f, cubeMesh](GfxContext* p) {

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
                gfxp->setTexture("u_srcHDRITex", HDRITex);

                cubeMesh->draw();

                gfxp->unbind();
            });
            pass.disableDepthTest();
            pass.render(GfxContext::get());
        }
    }

    GFX_API void RenderingUtils::drawWorldSpacePoint(GHTexture2D* dstTexture, GHDepthTexture* depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& p, const glm::vec4& color, f32 size)
    {
        static std::vector<PointCloud::Vertex> vertices(1);
        static std::vector<u32> indices{ 0 };
        static std::unique_ptr<Geometry> g = std::make_unique<PointCloud>(vertices, indices);
        static std::unique_ptr<GfxStaticSubMesh> mesh = std::unique_ptr<GfxStaticSubMesh>(GfxStaticSubMesh::create(std::string("drawWorldSpacePointMesh"), g.get()));

        vertices[0].pos = p;
        vertices[0].color = color;

        mesh->updateVertices(0, sizeOfVectorInBytes(vertices), vertices.data());

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "WorldSpacePointVS", ENGINE_SHADER_PATH "worldspace_point_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "VertexColorPS", ENGINE_SHADER_PATH "vertexcolor_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        const auto& desc = dstTexture->getDesc();
        RenderPass rp(desc.width, desc.height);
        RenderTarget rt(dstTexture);
        rt.bNeedsClear = false;
        rp.setRenderTarget(rt, 0);
        DepthTarget dt(depthTexture, false);
        rp.setDepthTarget(dt);
        rp.setRenderFunc([gfxp, dstTexture, viewMatrix, projectionMatrix, size](GfxContext* ctx) {
            gfxp->bind();
            gfxp->setUniform("u_viewMatrix", viewMatrix);
            gfxp->setUniform("u_projectionMatrix", projectionMatrix);
            gfxp->setUniform("u_pointSize", size);
            mesh->draw();
            gfxp->unbind();
        });
        rp.enableDepthTest();
        rp.render(GfxContext::get());
    }

    GFX_API void RenderingUtils::drawWorldSpaceLine(GHTexture2D* dstColorTexture, GHDepthTexture* depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
    {
        static std::vector<Lines::Vertex> vertices(2);
        static std::vector<u32> indices{ 0, 1 };
        static std::unique_ptr<Geometry> g = std::make_unique<Lines>(vertices, indices);
        static std::unique_ptr<GfxStaticSubMesh> mesh = std::unique_ptr<GfxStaticSubMesh>(GfxStaticSubMesh::create(std::string("drawWorldSpaceLineMesh"), g.get()));

        vertices[0].pos = start;
        vertices[0].color = color;
        vertices[1].pos = end;
        vertices[1].color = color;

        mesh->updateVertices(0, sizeOfVectorInBytes(vertices), vertices.data());

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "WorldSpaceLineVS", ENGINE_SHADER_PATH "worldspace_line_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "VertexColorPS", ENGINE_SHADER_PATH "vertexcolor_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        const auto& desc = dstColorTexture->getDesc();
        RenderPass rp(desc.width, desc.height);
        RenderTarget rt(dstColorTexture);
        rt.bNeedsClear = false;
        rp.setRenderTarget(rt, 0);
        DepthTarget dt(depthTexture, false);
        rp.setDepthTarget(dt);
        rp.setRenderFunc([gfxp, dstColorTexture, viewMatrix, projectionMatrix](GfxContext* ctx) {
            gfxp->bind();
            gfxp->setUniform("u_viewMatrix", viewMatrix);
            gfxp->setUniform("u_projectionMatrix", projectionMatrix);
            mesh->draw();
            gfxp->unbind();
        });
        rp.enableDepthTest();
        rp.render(GfxContext::get());
    }

    // todo: implement this
    GFX_API void RenderingUtils::drawScreenSpacePoint(GHTexture2D* dstTexture, const glm::vec3& p, const glm::vec4& color)
    {
        static std::vector<PointCloud::Vertex> vertices(1);
        static std::vector<u32> indices{ 0 };
        static std::unique_ptr<Geometry> g = std::make_unique<PointCloud>(vertices, indices);
        static std::unique_ptr<GfxStaticSubMesh> mesh = std::unique_ptr<GfxStaticSubMesh>(GfxStaticSubMesh::create(std::string("drawScreenSpacePointMesh"), g.get()));

        vertices[0].pos = p;

        mesh->updateVertices(0, sizeOfVectorInBytes(vertices), vertices.data());

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "WorldSpaceLineVS", ENGINE_SHADER_PATH "worldspace_line_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "VertexColorPS", ENGINE_SHADER_PATH "vertexcolor_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
    }

}