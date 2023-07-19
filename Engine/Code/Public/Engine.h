#pragma once

#include "Core.h"

#ifdef ENGINE_EXPORTS
    #define ENGINE_API __declspec(dllexport)
#else
    #define ENGINE_API __declspec(dllimport)
#endif

namespace Cyan 
{
    class App;
    class World;
    class GfxModule;

    class ENGINE_API Engine
    {
    public:
        ~Engine() { }

        static Engine* create(std::unique_ptr<App> app);
        static Engine* get();

        void initialize();
        void deinitialize();
        void run();
    private:
        Engine(std::unique_ptr<App> app); // hiding constructor to prohibit direct construction
        void update();
        bool syncWithRendering();
        void prepareForRendering();
        void submitForRendering();

        std::unique_ptr<GfxModule> m_gfx = nullptr;
        std::unique_ptr<App> m_app = nullptr;
        std::unique_ptr<World> m_world = nullptr;
        bool bRunning = false;
        i32 m_gameFrameNumber = 0;
        static Engine* s_instance;
    };
}
