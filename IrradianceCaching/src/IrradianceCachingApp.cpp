#include <thread>

#include "CyanApp.h"
#include "Scene.h"
#include "RayTracingScene.h"

class IrradianceCachingApp : public Cyan::DefaultApp
{
public:
    IrradianceCachingApp(u32 appWindowWidth, u32 appWindowHeight)
        : DefaultApp(appWindowWidth, appWindowHeight)
    {

    }

    ~IrradianceCachingApp() { }

    virtual void customInitialize() override
    {
        using namespace Cyan;
#if 1
        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/cornell_box.gltf", "CornellBox");
#else
        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/shader_balls.glb", "ShaderBall");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/sponza-gltf-pbr/sponza.glb", "Sponza");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/sun_temple/SimplifiedSunTemple.glb", "SimplifiedSunTemple");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/ue_archvis.glb", "interior");
#endif
        ITextureRenderable::Spec spec = { };
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.wrap_r = WM_WRAP;
        parameter.wrap_s = WM_WRAP;
        parameter.wrap_t = WM_WRAP;
        AssetManager::importTexture2D("BlueNoise_1024x1024", ASSET_PATH "textures/noise/LDR_RGBA_0.png", spec, parameter);
        AssetManager::importTexture2D("BlueNoise_16x16", ASSET_PATH "textures/noise/LDR_LLL1_0.png", spec, parameter);

        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(1.f, 1.5f, 2.5f), glm::vec4(1.f, 1.f, 1.f, 1.f));
        // sky light 
        auto skylight = m_scene->createSkyLight("SkyLight", ASSET_PATH "cubemaps/pisa.hdr");
        skylight->build();

        m_scene->update();

        // ray tracer
        m_rayTracer = std::make_unique<RayTracer>();
    }

    virtual void customFinalize() override
    {
        for (auto thread : spawnedThreads)
        {
            if (thread)
            {
                thread->join();
                delete thread;
            }
        }
    }

    virtual void customUpdate() override
    {
        using namespace Cyan;
        Renderer::get()->addUIRenderCommand([this]() {
            ImGui::Begin("RayTracing");
            {
                if (ImGui::CollapsingHeader("Ray Tracer", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::Button("Run"))
                    {
                        if (!m_rayTracer->busy())
                        {
                            spawnedThreads.push_back(new std::thread([this]() {
                                PerspectiveCamera* camera = static_cast<PerspectiveCamera*>(m_scene->camera->getCamera());
                                m_rayTracer->renderScene(*m_rtxScene, *camera, image, [this]() { });
                            }));
                        }
                    }
                    if (m_rayTracer->busy())
                    {
                        ImGui::SameLine();
                        ImGui::Text("Progress: %.2f\%", m_rayTracer->getProgress() * 100.f);
                    }
                    ImGui::Text("Ray Tracing Output"); ImGui::SameLine();
                    if (ImGui::Button("Clear"))
                    {
                        glClearTexImage(rayTracingOutput->getGpuResource(), 0, GL_RGB, GL_FLOAT, nullptr);
                        image.clear();
                    }
                    if (!rayTracingOutput)
                    {
                        ITextureRenderable::Spec spec = { };
                        spec.width = image.size.x;
                        spec.height = image.size.y;
                        spec.pixelFormat = PF_RGB16F;
                        rayTracingOutput = AssetManager::createTexture2D("RayTracingOutput", spec);
                    }
                    auto spec = rayTracingOutput->getTextureSpec();
                    glBindTexture(GL_TEXTURE_2D, rayTracingOutput->getGpuResource());
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, spec.width, spec.height, GL_RGB, GL_FLOAT, image.pixels.data());
                    glBindTexture(GL_TEXTURE_2D, 0);
                    ImGui::Image((void*)rayTracingOutput->getGpuResource(), ImVec2(spec.width, spec.height), ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
                }
            }
            ImGui::End();
        });
    }

    virtual void customRender() override
    {

    }

    void screenSpaceIrradianceCaching()
    {

    }

private:
    std::unique_ptr<Cyan::RayTracingScene> m_rtxScene = nullptr;
    std::unique_ptr<Cyan::RayTracer> m_rayTracer = nullptr;
    std::vector<std::thread*> spawnedThreads;
    Cyan::Image image = Cyan::Image(glm::uvec2(640, 360));
    Cyan::Texture2DRenderable* rayTracingOutput = nullptr;
};

// entry point
int main()
{
    IrradianceCachingApp* app = new IrradianceCachingApp(1280, 720);
    app->initialize();
    app->run();
    app->finalize();
    delete app;
    return 0;
}
