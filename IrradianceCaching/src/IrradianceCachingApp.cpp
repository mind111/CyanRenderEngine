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
        m_scene = std::make_shared<Cyan::Scene>("SimpleScene");
        AssetManager::importGltf(m_scene.get(), "C:/dev/cyanRenderEngine/asset/mesh/CornellBox.gltf", "CornellBox");

        // camera
#if 1
        m_scene->createPerspectiveCamera(
            /*name=*/"Camera",
            /*transform=*/Transform {
                glm::vec3(0.f, 3.2f, 8.f)
            },
            /*inLookAt=*/glm::vec3(0., 1., -1.),
            /*inWorldUp=*/glm::vec3(0.f, 1.f, 0.f),
            /*inFov=*/45.f,
            .5f,
            150.f,
            1280.f / 720.f
        );
#else
        m_scene->camera = { };
        m_scene->camera.position = glm::vec3(0.0, 3.2, 8.0);
        m_scene->camera.lookAt = glm::vec3(0.0, 1.0, -1.0);
        m_scene->camera.worldUp = glm::vec3(0.0, 1.0, 0.0);
        m_scene->camera.fov = 45.f;
        m_scene->camera.n = .5f;
        m_scene->camera.f = 150.f;
        m_scene->camera.projection = glm::perspective(m_scene->camera.fov, 1280.f / 720.f, m_scene->camera.n, m_scene->camera.f);
#endif
        // m_scene->createSunLight();

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
