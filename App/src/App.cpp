#include <thread>

#include "CyanApp.h"
#include "Scene.h"
#include "RayTracingScene.h"

class TheGame : public Cyan::DefaultApp
{
public:
    TheGame(u32 appWindowWidth, u32 appWindowHeight)
        : DefaultApp(appWindowWidth, appWindowHeight)
    {

    }

    ~TheGame() { }

    virtual void customInitialize() override
    {
        using namespace Cyan;

        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/shader_balls.glb", "ShaderBall");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/pica_pica_scene.glb", "PicaPica");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/cyan_toybox.glb");

        ITextureRenderable::Spec spec = { };
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.wrap_r = WM_WRAP;
        parameter.wrap_s = WM_WRAP;
        parameter.wrap_t = WM_WRAP;
        AssetManager::importTexture2D("BlueNoise_1024x1024", ASSET_PATH "textures/noise/LDR_RGBA_0.png", spec, parameter);
        AssetManager::importTexture2D("BlueNoise_16x16", ASSET_PATH "textures/noise/LDR_LLL1_0.png", spec, parameter);

        // skybox
        auto skybox = m_scene->createSkybox("Skybox", ASSET_PATH "cubemaps/neutral_sky.hdr", glm::uvec2(2048));
        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(1.3f, 1.3f, 0.5f), glm::vec4(0.88f, 0.77f, 0.65f, 50.f));
        // sky light 
        auto skylight = m_scene->createSkyLight("SkyLight", ASSET_PATH "cubemaps/neutral_sky.hdr");
        skylight->build();

        m_scene->update();
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
    TheGame* app = new TheGame(1024, 1024);
    app->initialize();
    app->run();
    app->deinitialize();
    delete app;
    return 0;
}
