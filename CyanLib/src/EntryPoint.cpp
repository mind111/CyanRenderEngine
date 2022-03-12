#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"

int main(int argc, char* argv[]) {

    std::cout << "---- Hello Cyan! ----" << std::endl;
    DemoApp* app = new DemoApp();

    glm::vec2 sceneRenderSize(1280.f, 720.f);
    glm::vec2 debugViewportSize(400.f, 720.f);
    f32 windowWidth = sceneRenderSize.x + debugViewportSize.x;
    f32 windowHeight = 720.f;
    app->initialize(windowWidth, windowHeight, glm::vec2(400.f, 0.f), sceneRenderSize);
    app->run();
    app->shutDown();
    delete app;

    return 0;
}
