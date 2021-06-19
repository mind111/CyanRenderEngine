#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"
// #include "PbrSpheres.h"

int main(int argc, char* argv[]) {

    std::cout << "---- Hello Cyan! ----" << std::endl;
    PbrApp* app = new PbrApp();
    app->init(1280, 960);
    app->run();
    app->shutDown();
    delete app;

    return 0;
}
