#include "CyanApp.h"

namespace Cyan
{
    DefaultApp::DefaultApp(u32 appWindowWidth, u32 appWindowHeight)
        : isRunning(true), m_appWindowDimension(appWindowWidth, appWindowHeight)
    {
        gEngine = std::make_unique<Engine>(appWindowWidth, appWindowHeight);
    }

    void DefaultApp::initialize()
    {
        gEngine->initialize();
        // create a default scene that can be modified or overwritten by custom app
        m_scene = std::make_shared<Scene>("DefaultScene", (f32)m_appWindowDimension.x / m_appWindowDimension.y);
        customInitialize();
    }

    void DefaultApp::deinitialize()
    {
        gEngine->deinitialize();
        customFinalize();
    }

    void DefaultApp::update()
    {
        gEngine->update(m_scene.get());
        customUpdate();
    }

    void DefaultApp::render()
    {
        gEngine->render();
        customRender();
    }
   
    void DefaultApp::run()
    {
        while (isRunning)
        {
            update();
            render();
        }
    }
}
