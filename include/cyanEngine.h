#pragma once 

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

    void setup(WindowConfig windowConfig, const char* sceneFolderPath=0);

    // TODO: This should be part of CyanDemo, engine should just provide functionalities
    // like gEngine->render(uint32 sceneIdx)

    void render(Scene& scene);
    void shutDown();
    Scene* loadScene(const char* filename);

    /* Gui */
    void displaySliderFloat(const char* title, float* address, float min, float max);
    void displayFloat3(const char* title, glm::vec3& value, bool isStatic=false);

    /* Input */
    void registerMouseCursorCallback(MouseCursorCallback* callback);
    void registerMouseButtonCallback(MouseButtonCallback* callback);
    void updateMouseCursorPosition(double x, double y);
    void processMouseButtonInput(int button, int action);

    /* Rendering */
    void updateShaderParams(ShaderBase* shader);

    const Window& getWindow() { return mWindow; } 
    CyanRenderer* getRenderer() { return mRenderer; }
    glm::vec2 getMouseCursorDelta() { return glm::vec2(float(cursorDeltaX), float(cursorDeltaY)); }
    void swapBuffers(); 

private:
    Window mWindow;

    /* These are recorded at sub frame precision */
    double cursorX, cursorY;
    double cursorDeltaX, cursorDeltaY;

    CyanRenderer* mRenderer;

    /* Misc (To be refactored) */
    MouseCursorCallback* mouseCursorCallback;
    std::vector<MouseButtonCallback*> mouseButtonCallbacks;
};
