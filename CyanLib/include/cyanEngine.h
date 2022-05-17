#pragma once 

#include "imgui/imgui.h"

#include "GraphicsSystem.h"
#include "IOSystem.h"
#include "Window.h"

// TODO: Move this to somewhere else
/* Input wrapper */
#define CYAN_PRESS GLFW_PRESS
#define CYAN_RELEASE GLFW_RELEASE
#define CYAN_MOUSE_BUTTON_RIGHT GLFW_MOUSE_BUTTON_RIGHT
#define CYAN_MOUSE_BUTTON_LEFT GLFW_MOUSE_BUTTON_LEFT

class DefaultApp;

typedef void MouseCursorCallback(double, double, double, double);
typedef void MouseButtonCallback(int, int);
typedef void MouseScrollWheelCallback(double, double);
typedef void KeyCallback(i32, i32);

namespace Cyan
{
    class Engine
    {
    public:
        Engine(u32 windowWidth, u32 windowHeight);
        ~Engine() { }

        static Engine* get() { return singleton; }

        void initialize();
        void update();
        void render();
        void finalize();

        /* Input */
        void registerMouseCursorCallback(MouseCursorCallback* callback);
        void registerMouseButtonCallback(MouseButtonCallback* callback);
        void registerMouseScrollWheelCallback(MouseScrollWheelCallback* callback);
        void registerKeyCallback(KeyCallback* callback);
        void updateMouseCursorPosition(double x, double y);
        void processMouseButtonInput(int button, int action);
        void processMouseScroll(double xOffset, double yOffset);
        void processKey(i32 key, i32 action);

        GLFWwindow* getAppWindow() { return m_graphicsSystem->getAppWindow(); }

    private:
        static Engine* singleton;
        IOSystem* m_IOSystem = nullptr;
        GraphicsSystem* m_graphicsSystem = nullptr;

        /* These are recorded at sub frame precision */
        double cursorX, cursorY;
        double cursorDeltaX, cursorDeltaY;

        /* Misc (To be refactored) */
        KeyCallback* keyCallback;
        MouseCursorCallback* mouseCursorCallback;
        MouseScrollWheelCallback* mouseScrollCallback;
        std::vector<MouseButtonCallback*> mouseButtonCallbacks;
    };
}
