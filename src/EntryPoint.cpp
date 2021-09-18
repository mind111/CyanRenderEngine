#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"
// #include "PbrSpheres.h"

int main(int argc, char* argv[]) {

    std::cout << "---- Hello Cyan! ----" << std::endl;
    PbrApp* app = new PbrApp();

    glm::vec2 sceneRenderSize(1280.f, 720.f);
    glm::vec2 debugViewportSize(400.f, 720.f);
    f32 windowWidth = sceneRenderSize.x + debugViewportSize.x;
    f32 windowHeight = std::max(sceneRenderSize.y, debugViewportSize.y);
    app->init(windowWidth, windowHeight, glm::vec2(400.f, 0.f), sceneRenderSize);
    app->run();
    app->shutDown();
    delete app;

    return 0;
}
