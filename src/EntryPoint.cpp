#include <iostream>

#include "CyanEngine.h"
#include "BasicApp.h"

int main(int argc, char* argv[]) {

    std::cout << "---- Hello Cyan! ----" << std::endl;
    BasicApp* app = new BasicApp();
    app->init(800, 600);
    app->run();
    app->shutDown();
    delete app;

    return 0;
}
