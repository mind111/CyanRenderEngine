#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"
// #include "PbrSpheres.h"

int main(int argc, char* argv[]) {

    std::cout << "---- Hello Cyan! ----" << std::endl;
    PbrApp* app = new PbrApp();

    glm::ivec2 sceneViewportSize(1280, 960);
    glm::ivec2 debugViewportSize(400, 960);
    u32 windowWidth = sceneViewportSize.x + debugViewportSize.x;
    u32 windowHeight = std::max(sceneViewportSize.y, debugViewportSize.y);
    // app->init(1280, 960);
    app->init(windowWidth, windowHeight, sceneViewportSize.x, sceneViewportSize.y);
    app->run();
    app->shutDown();
    delete app;

    return 0;
}
