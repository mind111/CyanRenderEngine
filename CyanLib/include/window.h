#pragma once

#include "glfw3.h"

namespace Cyan
{
    struct Window 
    {
        GLFWwindow* mpWindow;
        double lastX, lastY;
        int width, height;
        int keys[1024];
    };

    class WindowManager {
    public:
        WindowManager() {}
        static void mousePosCallBack(GLFWwindow* window, double cursorX, double cursorY);
        static void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
        static void keyCallBack(GLFWwindow* window, int key, int scanCode, int action, int mods);
        static void initWindow(Window& window);
    };

    extern WindowManager windowManager;
}
