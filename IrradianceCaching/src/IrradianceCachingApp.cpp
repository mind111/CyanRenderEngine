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

        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/simplified_cyan_toybox.glb", "CyanToybox");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/shader_balls.glb", "ShaderBall");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/cornell_box.gltf", "CornellBox");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/sponza-gltf-pbr/sponza.glb", "Sponza");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/ue_archvis.glb", "interior");
        // AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/sun_temple/SimplifiedSunTemple.glb", "SimplifiedSunTemple");

        ITextureRenderable::Spec spec = { };
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.wrap_r = WM_WRAP;
        parameter.wrap_s = WM_WRAP;
        parameter.wrap_t = WM_WRAP;
        AssetManager::importTexture2D("BlueNoise_1024x1024", ASSET_PATH "textures/noise/LDR_RGBA_0.png", spec, parameter);
        AssetManager::importTexture2D("BlueNoise_16x16", ASSET_PATH "textures/noise/LDR_LLL1_0.png", spec, parameter);

        // skybox
        auto skybox = m_scene->createSkybox("Skybox", ASSET_PATH "cubemaps/pisa.hdr", glm::uvec2(1024));
        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(1.f, 1.5f, 2.5f), glm::vec4(1.f, 1.f, 1.f, 11.f));
        // sky light 
        auto skylight = m_scene->createSkyLight("SkyLight", ASSET_PATH "cubemaps/pisa.hdr");
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
    IrradianceCachingApp* app = new IrradianceCachingApp(1280, 720);
    app->initialize();
    app->run();
    app->finalize();
    delete app;
    return 0;
}
