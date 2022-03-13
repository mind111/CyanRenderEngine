#pragma once 

#include "imgui/imgui.h"

#include "GraphicsSystem.h"
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

typedef void MouseCursorCallback(double, double, double, double);
typedef void MouseButtonCallback(int, int);
typedef void MouseScrollWheelCallback(double, double);
typedef void KeyCallback(i32, i32);

class CyanEngine
{
public:
    CyanEngine();
    ~CyanEngine() { }

    void initialize(WindowConfig windowConfig, glm::vec2 viewportPos, glm::vec2 renderSize);
    void finalize();

    /* Gui */
    void displaySliderFloat(const char* title, float* address, float min, float max);
    void displayFloat3(const char* title, glm::vec3& value, bool isStatic=false);

    /* Input */
    void processInput();
    void registerMouseCursorCallback(MouseCursorCallback* callback);
    void registerMouseButtonCallback(MouseButtonCallback* callback);
    void registerMouseScrollWheelCallback(MouseScrollWheelCallback* callback);
    void registerKeyCallback(KeyCallback* callback);
    void updateMouseCursorPosition(double x, double y);
    void processMouseButtonInput(int button, int action);
    void processMouseScroll(double xOffset, double yOffset);
    void processKey(i32 key, i32 action);

    const Window& getWindow() { return m_window; } 
    glm::vec2 getSceneViewportPos();
    glm::vec2 getMouseCursorDelta() { return glm::vec2(float(cursorDeltaX), float(cursorDeltaY)); }
    void swapBuffers(); 

    // GfxSystem
    Cyan::GraphicsSystem* m_graphicsSystem;

private:
    Window m_window;
    // scene viewport origin in screen space
    glm::vec2 m_sceneViewportPostion;
    // default render target size for the renderer
    glm::vec2 m_renderSize;

    /* These are recorded at sub frame precision */
    double cursorX, cursorY;
    double cursorDeltaX, cursorDeltaY;

    /* Misc (To be refactored) */
    KeyCallback* keyCallback;
    MouseCursorCallback* mouseCursorCallback;
    MouseScrollWheelCallback* mouseScrollCallback;
    std::vector<MouseButtonCallback*> mouseButtonCallbacks;
};
