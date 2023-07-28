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

        void onSceneCameraAdded(SceneCamera* camera);
        void onSceneCameraRemoved(SceneCamera* camera);

        void onStaticMeshInstanceAdded(StaticMeshInstance* staticMeshInstance);
        void onStaticMeshInstanceRemoved(StaticMeshInstance* staticMeshInstance);
        void onStaticMeshInstanceTransformUpdated(StaticMeshInstance* staticMeshInstance);

    private:
        Engine(std::unique_ptr<App> app); // hiding constructor to prohibit direct construction

        void update();
        void handleInputs();
        void enqueueFrameGfxTask(const FrameGfxTask& task);
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
        std::vector<glm::mat4> m_transformCache;
        std::unordered_map<std::string, std::vector<u32>> m_staticSubMeshInstanceMap;
        std::queue<u32> m_emptyStaticSubMeshInstanceSlots;

        /* These are accessed on the render thread */
        std::unique_ptr<Scene> m_sceneRenderThread = nullptr;
        std::vector<SceneView*> m_views;

        bool bRunning = false;
        i32 m_mainFrameNumber = 0;
        static Engine* s_instance;
    };
}
