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

    void DefaultApp::finalize()
    {
        gEngine->finalize();
        customFinalize();
    }

    void DefaultApp::update()
    {
        gEngine->setScene(m_scene);
        gEngine->update();
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
