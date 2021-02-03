#pragma once

#include "glfw3.h"

struct Window {
    GLFWwindow* mpWindow;
    double lastX, lastY;
    int width, height;
    int keys[1024];
};

class WindowManager {
public:
    WindowManager() {}
    static void mousePosCallBack(GLFWwindow* window, double xPos, double yPos);
    static void mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
    static void keyCallBack(GLFWwindow* window, int key, int scanCode, int action, int mods);
    static void initWindow(Window& window);
};

extern WindowManager windowManager;