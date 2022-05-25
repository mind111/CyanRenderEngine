#include "CyanApp.h"

namespace Cyan
{
    DefaultApp::DefaultApp(u32 appWindowWidth, u32 appWindowHeight)
        : isRunning(true)
    {
        gEngine = std::make_unique<Engine>(appWindowWidth, appWindowHeight);
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
        gEngine->setScene(m_scene);
        gEngine->update();
        customUpdate();
    }

    void DefaultApp::render()
    {
        gEngine->beginRender();
        gEngine->render();
        customRender();
        gEngine->endRender();
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
