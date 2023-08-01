#include <atomic>

#include "glfw3.h"

#include "App.h"
#include "Engine.h"
#include "World.h"
#include "SceneCamera.h"
#include "StaticMesh.h"
#include "AssetManager.h"
#include "InputManager.h"

#include "GfxModule.h"
#include "GfxInterface.h"
#include "Scene.h"

// todo: get material back
// todo: think about component memory ownership
// todo: think about asset ownership
// todo: gfx side material representation
namespace Cyan
{
    static std::queue<FrameGfxTask> s_frameGfxTaskQueue;

    Engine* Engine::s_instance = nullptr;
    Engine::~Engine() 
    {

    }

    Engine* Engine::create(std::unique_ptr<App> app)
    {
        static std::unique_ptr<Engine> engine(new Engine(std::move(app)));
        s_instance = engine.get();
        return s_instance;
    }

    Engine* Engine::get()
    {
        return s_instance;
    }

    Engine::Engine(std::unique_ptr<App> app)
    {
        m_gfx = std::move(GfxModule::create(glm::uvec2(app->m_windowWidth, app->m_windowHeight)));
        m_app = std::move(app);
        m_inputManager = InputManager::get();
        m_world = std::make_unique<World>("Default");
    }

    void Engine::initialize()
    {
        m_assetManager = AssetManager::get();
        m_gfx->initialize();
        // setup input handler with the render thread
        auto inputHandler = std::make_unique<InputEventHandler>([this](InputEventQueue& eventQueue) {
            // this lambda will only be invoked on the render thread
            assert(GfxModule::isInRenderThread());
            std::lock_guard<std::mutex> lock(m_frameInputEventsQueueMutex);
            m_frameInputEventsQueue.push(std::move(eventQueue));
            assert(m_frameInputEventsQueue.size() <= 2);
        });
        m_gfx->setInputEventHandler(std::move(inputHandler));

        m_app->initialize(m_world.get());
    }

    void Engine::deinitialize()
    {
    }

    void Engine::update()
    {
        m_app->update(m_world.get());
        m_world->update();
    }

    void Engine::handleInputs()
    {
        InputEventQueue q;
        {
            std::lock_guard<std::mutex> lock(m_frameInputEventsQueueMutex);
            if (!m_frameInputEventsQueue.empty())
            {
                q = std::move(m_frameInputEventsQueue.front());
                m_frameInputEventsQueue.pop();
            }
        }

        // process input events
        while (!q.empty())
        {
            std::unique_ptr<ILowLevelInputEvent> event = std::move(q.front());
            q.pop();
            switch (event->getEventType())
            {
                case ILowLevelInputEvent::Type::kKeyEvent: 
                {
                    LowLevelKeyEvent* keyEvent = static_cast<LowLevelKeyEvent*>(event.get());
                    m_inputManager->processKeyEvent(keyEvent);
                    break;
                }
                case ILowLevelInputEvent::Type::kMouseCursor:
                {
                    LowLevelMouseCursorEvent* mouseCursorEvent = static_cast<LowLevelMouseCursorEvent*>(event.get());
                    m_inputManager->processMouseCursorEvent(mouseCursorEvent);
                    break;
                }
                case ILowLevelInputEvent::Type::kMouseButton:
                {
                    LowLevelMouseButtonEvent* mouseButtonEvent = static_cast<LowLevelMouseButtonEvent*>(event.get());
                    m_inputManager->processMouseButtonEvent(mouseButtonEvent);
                    break;
                }
                default: break;
            }
        }
    }

    bool Engine::syncWithRendering()
    {
        i32 renderFrameNumber = m_gfx->m_renderFrameNumber.load();
        assert(renderFrameNumber <= m_mainFrameNumber);
        return renderFrameNumber >= (m_mainFrameNumber - 1);
    }

    void Engine::submitForRendering()
    {
        World* world = m_world.get();

        struct CameraState
        {
            CameraViewInfo m_cameraViewInfo;
            bool bPower = true;
            u32 m_numRenderedFrames = 0u;
            f32 m_elapsedTime = 0.f;
            f32 m_deltaTime = 0.f;
            glm::uvec2 m_resolution;
            SceneCamera::RenderMode m_renderMode;
        };

        u32 numCameras = (u32)world->m_cameras.size();
        std::vector<CameraState> cameraStates(numCameras);
        for (u32 i = 0; i < numCameras; ++i)
        {
            cameraStates[i].m_cameraViewInfo = world->m_cameras[i]->m_cameraViewInfo;
            cameraStates[i].bPower = world->m_cameras[i]->bPower;
            cameraStates[i].m_numRenderedFrames = world->m_cameras[i]->m_numRenderedFrames;
            cameraStates[i].m_elapsedTime = world->m_cameras[i]->m_elapsedTime;
            cameraStates[i].m_deltaTime = world->m_cameras[i]->m_deltaTime;
            cameraStates[i].m_resolution = world->m_cameras[i]->m_resolution;
            cameraStates[i].m_renderMode = world->m_cameras[i]->m_renderMode;
        }

        FrameGfxTask task = { };
        task.debugName = std::string("SyncRenderState");
        task.lambda = [this, cameraStates](Frame& frame) {
            // update scene views
            std::vector<SceneView*> views = *frame.views;
            for (i32 i = 0; i < views.size(); ++i)
            {
                const auto& cameraState = cameraStates[i];
                views[i]->m_viewMode = (SceneView::ViewMode)cameraState.m_renderMode;
                auto& viewState = views[i]->m_state;
                if (viewState.frameCount > 0)
                {
                    viewState.prevFrameViewMatrix = viewState.viewMatrix;
                    viewState.prevFrameProjectionMatrix = viewState.projectionMatrix;
                    viewState.prevFrameCameraPosition = viewState.cameraPosition;
                }

                // todo: detect resolution changes
                viewState.resolution = cameraState.m_resolution;

                viewState.aspectRatio = cameraState.m_cameraViewInfo.m_perspective.aspectRatio;
                viewState.viewMatrix = cameraState.m_cameraViewInfo.viewMatrix();
                viewState.projectionMatrix = cameraState.m_cameraViewInfo.projectionMatrix();
                viewState.cameraPosition = cameraState.m_cameraViewInfo.m_worldSpacePosition;
                viewState.cameraRight = cameraState.m_cameraViewInfo.worldSpaceRight();
                viewState.cameraForward = cameraState.m_cameraViewInfo.worldSpaceForward();
                viewState.cameraUp = cameraState.m_cameraViewInfo.worldSpaceUp();
                viewState.frameCount = cameraState.m_numRenderedFrames;
                viewState.elapsedTime = cameraState.m_elapsedTime;
                viewState.deltaTime = cameraState.m_deltaTime;
            }
        };

        enqueueFrameGfxTask(task);

        Frame frame = { };
        frame.simFrameNumber = m_mainFrameNumber;
        frame.gfxTasks = std::move(s_frameGfxTaskQueue);
        frame.scene = m_world->m_sceneRenderThread.get();
        frame.views = &m_world->m_views;

        GfxModule::enqueueFrame(frame);
    }

    void Engine::run()
    {
        bRunning = true;
        while (bRunning)
        {
            // check if main thread is too far ahead of render thread
            if (syncWithRendering())
            {
                handleInputs();

                // simulating a frame
                // cyanInfo("Simulating frame %d", m_mainFrameNumber);
                // do sim work on main thread
                update();

                // copy game state and submit work to render thread
                submitForRendering();

                m_mainFrameNumber++;

#if !THREADED_RENDERING
                m_gfx->renderOneFrame();
#endif
            }
        }
    }

    void Engine::enqueueFrameGfxTask(const FrameGfxTask& task)
    {
        s_frameGfxTaskQueue.push(task);
    }
}
