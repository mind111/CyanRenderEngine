#include <filesystem>
#include <array>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanApp.h"
#include "Texture.h"
#include "TextureAtlas.h"

class TexturePackingApp : public Cyan::DefaultApp
{
public:
    TexturePackingApp(u32 width, u32 height)
        : DefaultApp(width, height)
    {

    }

    virtual void customInitialize() override
    {
        {
            glm::uvec2 resolution = gEngine->getGraphicsSystem()->getAppWindowDimension();
            Cyan::ITexture::Spec spec = { };
            spec.width = resolution.x;
            spec.height = resolution.y;
            spec.type = TEX_2D;
            spec.pixelFormat = PF_RGB16F;
            m_sceneRenderingOutput = new Cyan::Texture2D("FrameOutput", spec);
        }

        // load test images
        for (const auto& entry : std::filesystem::directory_iterator("C:\\dev\\cyanRenderEngine\\TexturePacking\\data"))
        {
            std::cout << entry.path() << std::endl;
            std::string filename = entry.path().string();
            Cyan::Image image;
            i32 hdr = stbi_is_hdr(filename.c_str());
            if (hdr)
            {
                image.bitsPerChannel = 32;
                assert(0);
            }
            else
            {
                i32 is16Bit = stbi_is_16_bit(filename.c_str());
                if (is16Bit)
                {
                    image.pixels = std::shared_ptr<u8>((u8*)stbi_load_16(filename.c_str(), &image.width, &image.height, &image.numChannels, 0));
                    image.bitsPerChannel = 16;
                }
                else
                {
                    image.pixels = std::shared_ptr<u8>(stbi_load(filename.c_str(), &image.width, &image.height, &image.numChannels, 0));
                    image.bitsPerChannel = 8;
                }
            }
            assert(image.width == image.height);
            assert(Cyan::isPowerOf2(image.width));
            images.push_back(image);
        }

        textureAtlas = std::make_unique<Cyan::Texture2DAtlas>(16 * 1024, PF_RGBA8);

        for (i32 i = 0; i < images.size(); ++i)
        {
            if (!textureAtlas->pack(images[i]))
            {
                printf("Failed to pack image %s \n", images[i].name.c_str());
            }
        }
    }

    virtual void customRender(Cyan::Renderer* renderer, Cyan::Texture2D* sceneRenderingOutput) override
    {
        static i32 mipLevel = 0;
        renderer->addUIRenderCommand([this]() {
                ImGui::Begin("Texture Packing Tool", nullptr);
                ImGui::SliderInt("mip", &mipLevel, 0, textureAtlas->atlas->numMips);
                ImGui::End();
            }
        );
        auto renderTarget= renderer->createCachedRenderTarget("AppCustomRenderTarget", sceneRenderingOutput->width, sceneRenderingOutput->height);
        renderTarget->setColorBuffer(sceneRenderingOutput, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(.2f, .2f, .2f, 1.f));
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);
        renderer->drawFullscreenQuad(renderTarget, pipeline, [this](Cyan::VertexShader* vs, Cyan::PixelShader* ps) {
            ps->setTexture("srcTexture", textureAtlas->atlas.get());
            ps->setUniform("mip", mipLevel);
        });
        renderer->renderToScreen(sceneRenderingOutput);
    }

    std::vector<Cyan::Image> images;
    std::unique_ptr<Cyan::Texture2DAtlas> textureAtlas = nullptr;
};

int main()
{
    std::unique_ptr<TexturePackingApp> app = std::make_unique<TexturePackingApp>(1024, 1024);
    app->run();
    return 0;
}