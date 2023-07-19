#include "Core.h"

#include <atomic>

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
    class GfxModule
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
