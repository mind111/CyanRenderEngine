#include "GfxModule.h"

namespace Cyan
{
    GfxModule* GfxModule::s_instance = nullptr;
    GfxModule::GfxModule()
        : m_renderFrameNumber(0)
    {

    }

    GfxModule::~GfxModule()
    {

    }

    std::unique_ptr<GfxModule> GfxModule::create()
    {
        static std::unique_ptr<GfxModule> s_gfx(new GfxModule());
        if (s_instance != nullptr)
        {
            s_instance = s_gfx.get();
        }
        return std::move(s_gfx);
    }

    GfxModule* GfxModule::get()
    {
        return s_instance;
    }

    void GfxModule::initialize()
    {
        // launch a thread to run renderLoop()
    }

    void GfxModule::deinitialize()
    {

    }

    void GfxModule::renderLoop()
    {
        static bool bRunning = true;
        while (bRunning)
        {
        }
    }
}
