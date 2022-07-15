#if 0
#include <iostream>

#include "CyanEngine.h"
#include "PbrApp.h"

int main(int argc, char* argv[]) {

    cyanInfo("Initializing ... ")
#if 0
    DemoApp* app = new DemoApp(1280u, 720u);
#else

#endif
    app->initialize();
    app->run();
    app->finalize();
    delete app;
    return 0;
}
#endif
