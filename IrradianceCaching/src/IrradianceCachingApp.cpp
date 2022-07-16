#include "CyanApp.h"
#include "Scene.h"

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
        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/CornellBox.gltf", "CornellBox");
        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(1.f), glm::vec4(1.f, 1.f, 1.f, 30.f));
    }

    virtual void customFinalize() override
    {

    }

    virtual void customUpdate() override
    {

    }

    virtual void customRender() override
    {

    }
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
