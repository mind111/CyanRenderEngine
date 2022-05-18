#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"

int main(int argc, char* argv[]) {

    cyanInfo("Initializing ... ")
    DemoApp* app = new DemoApp(1280u, 720u);
    app->initialize();
    app->run();
    app->finalize();
    delete app;
    return 0;
}
