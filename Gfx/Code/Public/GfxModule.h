#include <atomic>

#include "Core.h"
#include "Gfx.h"

namespace Cyan
{
    struct RenderState
    {
    };

    struct Frame
    {
        u32 simFrameNumber;
    };

    class RenderQueue
    {
    public:
    private:
    };

    /**
     * the Gfx module responsible for everything rendering related
     */
    class GFX_API GfxModule
    {
    public:
        friend class Engine;
        ~GfxModule();
        static std::unique_ptr<GfxModule> create();
        static GfxModule* get();

        void initialize();
        void deinitialize();
    private:
        GfxModule();
        void renderLoop();

        std::atomic<i32> m_renderFrameNumber;
        static GfxModule* s_instance;
    };
}
