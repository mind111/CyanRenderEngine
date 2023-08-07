// define which app to run here ...
#define RAY_MARCHING_HEIGHT_FIELD 1
#define DEMO 0

#define CREATE_APP_INTERNAL(appClass, appWidth, appHeight)  \
    std::make_unique<Cyan::appClass>(appWidth, appHeight)   \

#if RAY_MARCHING_HEIGHT_FIELD
    #include "RayMarchingHeightField/RayMarchingHeightField.h"
    #define CREATE_APP(w, h)                                 \
        CREATE_APP_INTERNAL(RayMarchingHeightFieldApp, w, h)    \

#elif DEMO
    #include "Demo/DemoApp.h"
    #define CREATE_APP(w, h)               \
        CREATE_APP_INTERNAL(DemoApp, w, h)    \

#else
    #include <cassert>
    assert(0);
#endif

int main()
{
    Cyan::Engine* engine = Cyan::Engine::create(std::move(CREATE_APP(1920, 1080)));
    engine->initialize();
    engine->run();
    engine->deinitialize();
    return 0;
}
