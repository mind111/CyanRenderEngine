#include <iostream>
#include <chrono>
#include "glew.h"
#include "glfw3.h"
#include "cyanRenderer.h"
#include "window.h"
#include "shader.h"
#include "scene.h"
#include "camera.h"
#include "mathUtils.h"

void handleInput(Window& window, Camera& camera, float deltaTime) {
    double cursorX, cursorY;
    glfwGetCursorPos(window.mpWindow, &cursorX, &cursorY);
    if (window.keys[GLFW_KEY_W]) {
        camera.processKeyboard(GLFW_KEY_W, deltaTime);
    }
    if (window.keys[GLFW_KEY_S]) {
        camera.processKeyboard(GLFW_KEY_S, deltaTime);
    }
    if (window.keys[GLFW_KEY_A]) {
        camera.processKeyboard(GLFW_KEY_A, deltaTime);
    }
    if (window.keys[GLFW_KEY_D]) {
        camera.processKeyboard(GLFW_KEY_D, deltaTime);
    }
    if (window.keys[GLFW_MOUSE_BUTTON_MIDDLE]) {
        camera.processMouse(cursorX - window.lastX, cursorY - window.lastY, deltaTime);
        camera.eMode = ControlMode::orbit;
    }
    if (window.keys[GLFW_MOUSE_BUTTON_RIGHT]) {
        camera.processMouse(cursorX - window.lastX, cursorY - window.lastY, deltaTime);
        camera.eMode = ControlMode::free;
    }
    window.lastX = cursorX;
    window.lastY = cursorY;
}

// TODO: @camera orbit-mode controls
// TODO: @clean up control-flow with enabling MSAA
int main(int argc, char* argv[]) {
    std::cout << "Hello Cyan!" << std::endl;
    if (!glfwInit()) {
        std::cout << "glfw init failed" << std::endl;
    }

    Window mainWindow = {
        nullptr, 0, 0, 0, 0,
        {0}
    };

    windowManager.initWindow(mainWindow);
    glfwMakeContextCurrent(mainWindow.mpWindow);
    glewInit();

    // Initialization
    CyanRenderer renderer;
    renderer.initRenderer();
    renderer.initShaders();
    Scene scene;
    sceneManager.loadSceneFromFile(scene, "scene/default_scene/scene_config.json");


    std::chrono::high_resolution_clock::time_point current, last = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> timeSpan; 

    while (!glfwWindowShouldClose(mainWindow.mpWindow)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //----- FPS -----------------
        current = std::chrono::high_resolution_clock::now();
        timeSpan = std::chrono::duration_cast<std::chrono::duration<float>>(current - last);
        last = current;
        //---------------------------

        //---- Input handling -------
        handleInput(mainWindow, scene.mainCamera, .06); // TODO: need to swap out the hard-coded value later
        //---------------------------

        //-- framebuffer operations --
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.defaultFBO);

        if (renderer.enableMSAA) {
            renderer.setupMSAA();
        }
        //----------------------------

        //----- Draw calls -----------
        //----- Default first pass ---
        renderer.drawScene(scene);
        //----------------------------

        if (renderer.enableMSAA) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.MSAAFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer.intermFBO);
            glBlitFramebuffer(0, 0, 800, 600, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        } else {

        }

        renderer.bloomPostProcess();
        renderer.blitBackbuffer();

        glfwSwapBuffers(mainWindow.mpWindow);
        glfwPollEvents();
    }
    return 0;
}