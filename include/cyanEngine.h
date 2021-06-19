#pragma once 

#include "imgui/imgui.h"

#include "CyanRenderer.h"
#include "Window.h"

// TODO: Move this to somewhere else
/* Input wrapper */
#define CYAN_PRESS GLFW_PRESS
#define CYAN_RELEASE GLFW_RELEASE
#define CYAN_MOUSE_BUTTON_RIGHT GLFW_MOUSE_BUTTON_RIGHT
#define CYAN_MOUSE_BUTTON_LEFT GLFW_MOUSE_BUTTON_LEFT

class CyanApp;

struct MemoryArena
{

};

class MemoryManager
{

};

struct WindowConfig
{
    int width;
    int height;
};

typedef void MouseCursorCallback(double, double);
typedef void MouseButtonCallback(int, int);

class CyanEngine
{
public:
    CyanEngine();
    ~CyanEngine() { }

    void init(WindowConfig windowConfig, const char* sceneFolderPath=0);
    void shutDown();

    /* Gui */
    void displaySliderFloat(const char* title, float* address, float min, float max);
    void displayFloat3(const char* title, glm::vec3& value, bool isStatic=false);

    /* Input */
    void processInput();
    void registerMouseCursorCallback(MouseCursorCallback* callback);
    void registerMouseButtonCallback(MouseButtonCallback* callback);
    void updateMouseCursorPosition(double x, double y);
    void processMouseButtonInput(int button, int action);

    const Window& getWindow() { return m_window; } 
    Cyan::Renderer* getRenderer() { return m_renderer; }
    glm::vec2 getMouseCursorDelta() { return glm::vec2(float(cursorDeltaX), float(cursorDeltaY)); }
    void swapBuffers(); 

    /* Renderer */
    Cyan::Renderer* m_renderer;

private:
    Window m_window;

    /* These are recorded at sub frame precision */
    double cursorX, cursorY;
    double cursorDeltaX, cursorDeltaY;

    /* Misc (To be refactored) */
    MouseCursorCallback* mouseCursorCallback;
    std::vector<MouseButtonCallback*> mouseButtonCallbacks;
};
