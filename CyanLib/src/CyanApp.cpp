#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanApp.h"
#include "World.h"

namespace Cyan
{
    DefaultApp::DefaultApp(u32 appWindowWidth, u32 appWindowHeight)
        : isRunning(true), m_appWindowDimension(appWindowWidth, appWindowHeight)
    {
        m_engine = std::make_unique<Engine>(appWindowWidth, appWindowHeight);
        m_world = std::make_shared<World>("DefaultWorld");
    }

    void DefaultApp::initialize()
    {
        m_engine->initialize();

#if 0
        // setup default I/O controls 
        auto InputSystem = gEngine->getIOSystem();
        InputSystem->addIOEventListener<MouseCursorEvent>([this, InputSystem](f64 xPos, f64 yPos) {
            glm::dvec2 mouseCursorChange = InputSystem->getMouseCursorChange();
            if (InputSystem->isMouseRightButtonDown())
            {
                // In radians per pixel 
                const float kCameraOrbitSpeed = 0.005f;
                const float kCameraRotateSpeed = 0.005f;
                // todo: do the correct trigonometry to covert distance traveled in screen space into angle of camera rotation
                float phi = mouseCursorChange.x * kCameraOrbitSpeed; 
                float theta = mouseCursorChange.y * kCameraOrbitSpeed;
                m_scene->m_mainCamera->orbit(phi, theta);
            }
        });

        InputSystem->addIOEventListener<MouseButtonEvent>([this, InputSystem](i32 button, i32 action) {
            switch(button)
            {
                case CYAN_MOUSE_BUTTON_RIGHT:
                {
                    if (action == CYAN_PRESS)
                    {
                        InputSystem->mouseRightButtonDown();
                    }
                    else if (action == CYAN_RELEASE)
                    {
                        InputSystem->mouseRightButtonUp();
                    }
                    break;
                }
                default:
                    break;
            }
        });

        InputSystem->addIOEventListener<MouseWheelEvent>([this](f64 xOffset, f64 yOffset) {
            const f32 speed = 0.3f;
            m_scene->m_mainCamera->zoom(speed * yOffset);
        });

        InputSystem->addIOEventListener<KeyEvent>([this](i32 key, i32 action) {
            switch (key)
            {
            case GLFW_KEY_W:
            {
                if (action == CYAN_PRESS || action == GLFW_REPEAT)
                    m_scene->m_mainCamera->moveForward();
                break;
            }
            case GLFW_KEY_A:
            {
                if (action == CYAN_PRESS || action == GLFW_REPEAT)
                    m_scene->m_mainCamera->moveLeft();
                break;
            }
            case GLFW_KEY_S:
            {
                if (action == CYAN_PRESS || action == GLFW_REPEAT)
                    m_scene->m_mainCamera->moveBack();
                break;
            }
            case GLFW_KEY_D:
            {
                if (action == CYAN_PRESS || action == GLFW_REPEAT)
                    m_scene->m_mainCamera->moveRight();
                break;
            }
            default:
                break;
            }
        });
#endif

        ImGui_ImplGlfw_InstallCallbacks(m_engine->getGraphicsSystem()->getAppWindow());

#if 0
        // create a default scene that can be modified or overwritten by custom app
        m_scene = std::make_shared<Scene>("DefaultScene", (f32)m_appWindowDimension.x / m_appWindowDimension.y);
#endif

        // setup a default rendering lambda which can be overwritten by child app
        m_renderOneFrame = [this](GfxTexture2D* renderingOutput) {
            auto renderer = Renderer::get();
            // scene rendering
            if (m_world != nullptr) 
            {
                auto scene = m_world->m_scene;
                renderer->render(scene.get());

                // assuming that renders[0] is the active render
                if (!scene->m_renders.empty())
                {
                    auto render = scene->m_renders[0];
                    renderer->renderToScreen(render->albedo());
                }
            }
            // UI rendering
            renderer->renderUI();
        };

        customInitialize();
    }

    void DefaultApp::deinitialize()
    {
        customFinalize();
        m_engine->deinitialize();
    }

    void DefaultApp::update()
    {
        m_engine->update();
        m_world->update();
        customUpdate();
    }

    void DefaultApp::render()
    {
        m_engine->render(m_renderOneFrame);
    }
   
    void DefaultApp::run()
    {
        initialize();
        while (isRunning)
        {
            update();
            render();
        }
        deinitialize();
    }
}
