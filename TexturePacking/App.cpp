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
        materialManager = std::make_unique<Cyan::MaterialManager>(16 * 1024);
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
            Cyan::Image image(filename.c_str());
            materialManager->pack(image);
        }
    }

    virtual void customRender(Cyan::Renderer* renderer, Cyan::Texture2D* sceneRenderingOutput) override
    {
        static i32 mipLevel = 0;
        static i32 index;
        renderer->addUIRenderCommand([this]() {
                ImGui::Begin("Texture Packing Tool", nullptr);
                ImGui::SliderInt("index", &index, 0, (i32)Cyan::MaterialManager::Format::kCount - 1);
                ImGui::SliderInt("mip", &mipLevel, 0, materialManager->atlases[index]->atlas->numMips);
                ImGui::End();
            }
        );
        auto framebuffer= renderer->createCachedFramebuffer("AppCustomFramebuffer", sceneRenderingOutput->width, sceneRenderingOutput->height);
        framebuffer->setColorBuffer(sceneRenderingOutput, 0);
        framebuffer->setDrawBuffers({ 0 });
        framebuffer->clearDrawBuffer(0, glm::vec4(.2f, .2f, .2f, 1.f));
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);
        renderer->drawFullscreenQuad(framebuffer, pipeline, [this](Cyan::VertexShader* vs, Cyan::PixelShader* ps) {
            ps->setTexture("srcTexture", materialManager->atlases[index]->atlas.get());
            ps->setUniform("mip", mipLevel);
        });
        renderer->renderToScreen(sceneRenderingOutput);
    }

    std::unique_ptr<Cyan::MaterialManager> materialManager = nullptr;
};

int main()
{
    std::unique_ptr<TexturePackingApp> app = std::make_unique<TexturePackingApp>(1024, 1024);
    app->run();
    return 0;
}