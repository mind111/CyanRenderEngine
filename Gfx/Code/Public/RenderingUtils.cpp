#include "stb_image.h"

#include "RenderingUtils.h"
#include "RenderPass.h"
#include "GfxModule.h"
#include "GfxStaticMesh.h"
#include "Geometry.h"

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
                s_instance->s_noiseTextures.blueNoise16x16_0 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_1_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_1 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_2_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_2 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_3_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_3 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_4_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_4 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_5_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_5 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_6_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_6 = GHTexture2D::create(desc);
            }
            {
                ImageInfo info = { };
                loadImage(info, s_instance->s_noiseTextures.blueNoise16x16_7_Path);

                GHTexture2D::Desc desc = GHTexture2D::Desc::create(info.width, info.height, 1, translatePF(info), info.pixels.get());
                s_instance->s_noiseTextures.blueNoise16x16_7 = GHTexture2D::create(desc);
            }
        }
    }

    void RenderingUtils::deinitialize()
    {

    }

    void RenderingUtils::renderScreenPass(const glm::uvec2& renderResolution, const RenderTargetSetupFunc& renderTargetSetupFunc, GfxPipeline* p, ShaderSetupFunc& shaderSetupFunc)
    {
        RenderPass rp(renderResolution.x, renderResolution.y);
        renderTargetSetupFunc(rp);
        rp.setRenderFunc([p, &shaderSetupFunc](GfxContext* ctx) {
            p->bind();
            shaderSetupFunc(p);
            s_unitQuadMesh->draw();
            p->unbind();
        });

        rp.disableDepthTest();
        rp.render(GfxContext::get());
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
}