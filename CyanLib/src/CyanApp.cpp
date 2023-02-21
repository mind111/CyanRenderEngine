#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

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

        // setup default I/O controls 
        auto IOSystem = gEngine->getIOSystem();
        IOSystem->addIOEventListener<MouseCursorEvent>([this, IOSystem](f64 xPos, f64 yPos) {
            glm::dvec2 mouseCursorChange = IOSystem->getMouseCursorChange();
            if (IOSystem->isMouseRightButtonDown())
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

        IOSystem->addIOEventListener<MouseButtonEvent>([this, IOSystem](i32 button, i32 action) {
            switch(button)
            {
                case CYAN_MOUSE_BUTTON_RIGHT:
                {
                    if (action == CYAN_PRESS)
                    {
                        IOSystem->mouseRightButtonDown();
                    }
                    else if (action == CYAN_RELEASE)
                    {
                        IOSystem->mouseRightButtonUp();
                    }
                    break;
                }
                default:
                    break;
            }
        });

        IOSystem->addIOEventListener<MouseWheelEvent>([this](f64 xOffset, f64 yOffset) {
            const f32 speed = 0.3f;
            m_scene->m_mainCamera->zoom(speed * yOffset);
        });

        IOSystem->addIOEventListener<KeyEvent>([this](i32 key, i32 action) {
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

        ImGui_ImplGlfw_InstallCallbacks(gEngine->getGraphicsSystem()->getAppWindow());

        // create a default scene that can be modified or overwritten by custom app
        m_scene = std::make_shared<Scene>("DefaultScene", (f32)m_appWindowDimension.x / m_appWindowDimension.y);

        // setup a default rendering lambda which can be overwritten by child app
        m_renderOneFrame = [this](GfxTexture2D* renderingOutput) {
            auto renderer = Renderer::get();
            // scene rendering
            if (m_scene) 
            {
                if (auto camera = dynamic_cast<PerspectiveCamera*>(m_scene->m_mainCamera->getCamera())) 
                {
                    SceneView mainSceneView(*m_scene, *camera,
                        [](Entity* entity) {
                            return entity->getProperties() | EntityFlag_kVisible;
                        },
                        renderingOutput, 
                        { 0, 0, renderingOutput->width, renderingOutput->height }
                    );
                    renderer->render(m_scene.get(), mainSceneView, glm::uvec2(renderingOutput->width, renderingOutput->height));
                }
            }
            // UI rendering
            renderer->renderUI();
        };

        customInitialize();
    }

    void DefaultApp::deinitialize()
    {
        gEngine->deinitialize();
        customFinalize();
    }

    void DefaultApp::update()
    {
        gEngine->update();
        m_scene->update();
        customUpdate();
    }

    void DefaultApp::render()
    {
        gEngine->render(m_renderOneFrame);
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
