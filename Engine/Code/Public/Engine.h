#pragma once

#include <mutex>

#include "Core.h"
#include "MathLibrary.h"

#ifdef ENGINE_EXPORTS
    #define ENGINE_API __declspec(dllexport)
#else
    #define ENGINE_API __declspec(dllimport)
#endif

namespace Cyan 
{
#define ASSET_PATH "C:/dev/cyanRenderEngine/asset/"

    class App;
    class World;
    class GfxModule;
    class SceneCamera;
    class StaticMeshInstance;
    class Scene;
    class SceneView;
    class AssetManager;
    class InputManager;
    struct FrameGfxTask;

    class ENGINE_API Engine
    {
    public:
        ~Engine();

        static Engine* create(std::unique_ptr<App> app);
        static Engine* get();

        void initialize();
        void deinitialize();
        void run();

        // void enqueueFrameGfxTask(const FrameGfxTask& task);
        void enqueueFrameGfxTask(const enum class RenderingStage& stage, const char* taskName, std::function<void(struct Frame&)>&& taskLambda);
    private:
        Engine(std::unique_ptr<App> app); // hiding constructor to prohibit direct construction

        void update();
        void handleInputs();
        bool syncWithRendering();
        void submitForRendering();

        std::unique_ptr<GfxModule> m_gfx = nullptr;
        std::unique_ptr<App> m_app = nullptr;
        AssetManager* m_assetManager = nullptr;
        InputManager* m_inputManager = nullptr;

        /**
         * Input handling
         */
        using InputEventQueue = std::queue<std::unique_ptr<struct ILowLevelInputEvent>>;
        std::mutex m_frameInputEventsQueueMutex;
        std::queue<InputEventQueue> m_frameInputEventsQueue;

        /**
         * There can only be one world and one scene alive at any given time
         */
        std::unique_ptr<World> m_world = nullptr;

        bool bRunning = false;
        i32 m_mainFrameNumber = 0;
        static Engine* s_instance;
    };
}

#define ENQUEUE_GFX_TASK(taskName, ...)                     \
    FrameGfxTask task = { };                                \
    task.debugName = std::move(taskName);                   \
    task.lambda = std::move(__VA_ARGS__);                   \
    Engine::get()->enqueueFrameGfxTask(task);               \
