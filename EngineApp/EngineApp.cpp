#define CREATE_APP_INTERNAL(appClass, appWidth, appHeight)  \
    std::make_unique<Cyan::appClass>(appWidth, appHeight)   \

// #define PARALLAX_OCCLUSION_MAPPING
#define DEMO
// #define SIMPLE_PARTICLES
// #define RAYMARCHING_HEIGHTFIELD

#if defined DEMO
    #include "Demo/DemoApp.h"
    #define CREATE_APP(w, h)                                \
        CREATE_APP_INTERNAL(DemoApp, w, h)                  \

#elif defined RAYMARCHING_HEIGHTFIELD
    #include "RayMarchingHeightField/RayMarchingHeightField.h"
    #define CREATE_APP(w, h)                                 \
        CREATE_APP_INTERNAL(RayMarchingHeightFieldApp, w, h) \

#elif defined SIMPLE_PARTICLES
    #include "SimpleParticles/SimpleParticles.h"
    #define CREATE_APP(w, h)                                 \
        CREATE_APP_INTERNAL(SimpleParticles, w, h)           \

#elif defined PARALLAX_OCCLUSION_MAPPING
    #include "ParallaxOcclusionMapping/ParallaxOcclusionMapping.h"
    #define CREATE_APP(w, h)                                            \
        CREATE_APP_INTERNAL(ParallaxOcclusionMapping, w, h)             \

#else
    #error "No active app defined!"
#endif

#include "App.h"

int main()
{
    Cyan::Engine* engine = Cyan::Engine::create(std::move(CREATE_APP(1920, 1080)));
    engine->initialize();
    engine->run();
    engine->deinitialize();
    return 0;
}
