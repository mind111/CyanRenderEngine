#include "window.h"

WindowManager windowManager;

void WindowManager::mousePosCallBack(GLFWwindow* window, double cursorX, double cursorY) {
    Window* pWindow = (Window*)glfwGetWindowUserPointer(window);
    pWindow->lastX = cursorX;
    pWindow->lastY = cursorY;
}

void WindowManager::mouseButtonCallBack(GLFWwindow* window, int button, int action, int mods) {
    Window* pWindow = (Window*)glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        pWindow->keys[button] = 1;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        pWindow->keys[button] = 1;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        pWindow->keys[button] = 0;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
        pWindow->keys[button] = 0;
    }
}

void WindowManager::keyCallBack(GLFWwindow* window, int key, int scanCode, int action, int mods) {
    Window* pWindow = (Window*)glfwGetWindowUserPointer(window);
    // Key press
    if (key == GLFW_KEY_W && action == GLFW_PRESS) {
        pWindow->keys[GLFW_KEY_W] = 1;
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        pWindow->keys[GLFW_KEY_S] = 1;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        pWindow->keys[GLFW_KEY_A] = 1;
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        pWindow->keys[GLFW_KEY_D] = 1;
    }

    // Key release
    if (key == GLFW_KEY_W && action == GLFW_RELEASE) {
        pWindow->keys[GLFW_KEY_W] = 0;
    }
    if (key == GLFW_KEY_S && action == GLFW_RELEASE) {
        pWindow->keys[GLFW_KEY_S] = 0;
    }
    if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
        pWindow->keys[GLFW_KEY_A] = 0;
    }
    if (key == GLFW_KEY_D && action == GLFW_RELEASE) {
        pWindow->keys[GLFW_KEY_D] = 0;
    }
}

void WindowManager::initWindow(Window& window) {
    window.mpWindow = glfwCreateWindow(800, 600, "cyanEngine", nullptr, nullptr);
    glfwSetKeyCallback(window.mpWindow, keyCallBack);
    glfwSetMouseButtonCallback(window.mpWindow, mouseButtonCallBack);
    glfwSetWindowUserPointer(window.mpWindow, &window);
}