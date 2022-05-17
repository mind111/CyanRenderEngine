#include "CyanApp.h"

namespace Cyan
{
    DefaultApp::DefaultApp(u32 appWindowWidth, u32 appWindowHeight)
        : isRunning(true)
    {
        gEngine = new Engine(appWindowWidth, appWindowHeight);
        m_scene = new Scene;
    }

    void DefaultApp::initialize()
    {
        gEngine->initialize();
        customInitialize();
    }

    void DefaultApp::finalize()
    {
        gEngine->finalize();
        customFinalize();
    }

    void DefaultApp::update()
    {
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
