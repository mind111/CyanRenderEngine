#include "Core.h"
#include "GfxHardwareContext.h"
#include "GLContext.h"

namespace  Cyan
{
    GfxHardwareAPI getActiveGfxHardwareAPI()
    {
        static GfxHardwareAPI s_activeGfxHardwareAPI = GfxHardwareAPI::OpenGL;
        return s_activeGfxHardwareAPI;
    }

    GfxHardwareContext* GfxHardwareContext::s_instance = nullptr;
    GfxHardwareContext* Cyan::GfxHardwareContext::create()
    {
        if (s_instance == nullptr)
        {
            GfxHardwareAPI activeAPI = getActiveGfxHardwareAPI();

            switch (activeAPI)
            {
            case OpenGL: 
                s_instance = new GLGHContext();
                break;
            case Vulkan: 
                s_instance = nullptr;
                break;
            default: assert(0);
            }
        }
        return s_instance;
    }

    GfxHardwareContext* GfxHardwareContext::get()
    {
        assert(s_instance != nullptr);
        return s_instance;
    }
}
