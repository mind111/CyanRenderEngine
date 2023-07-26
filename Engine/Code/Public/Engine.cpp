#include <atomic>

#include "App.h"
#include "Engine.h"
#include "World.h"
#include "SceneCamera.h"
#include "StaticMesh.h"
#include "AssetManager.h"

#include "GfxModule.h"
#include "GfxInterface.h"
#include "Scene.h"

namespace Cyan
{
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
        m_world = std::make_unique<World>("Default");
        m_sceneRenderThread = std::make_unique<Scene>();
    }

    void Engine::initialize()
    {
        m_assetManager = AssetManager::get();
        m_gfx->initialize();
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

    bool Engine::syncWithRendering()
    {
        i32 renderFrameNumber = m_gfx->m_renderFrameNumber.load();
        assert(renderFrameNumber <= m_mainFrameNumber);
        return renderFrameNumber >= (m_mainFrameNumber - 1);
    }

    void Engine::submitForRendering()
    {
        World* world = m_world.get();
        const std::vector<glm::mat4>& transforms = m_transformCache;

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

        enqueueFrameGfxTask([this, cameraStates, transforms](Frame& frame) {
            // update scene views
            std::vector<SceneView*> views = *frame.views;
            for (i32 i = 0; i < views.size(); ++i)
            {
                const auto& cameraState = cameraStates[i];
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

            // copy scene state
            u32 numSubMeshInstances = (u32)frame.scene->m_staticSubMeshInstances.size();
            assert(numSubMeshInstances == (u32)transforms.size());
            for (u32 i = 0; i < numSubMeshInstances; ++i)
            {
                frame.scene->m_staticSubMeshInstances[i].localToWorldMatrix = transforms[i];
            }
        });

        Frame frame = { };
        frame.simFrameNumber = m_mainFrameNumber;
        frame.gfxTasks = std::move(m_frameGfxTaskQueue);
        frame.scene = m_sceneRenderThread.get();
        frame.views = &m_views;

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
                // rendering a frame
                printf("[Main   Thread] Simulating frame %d \n", m_mainFrameNumber);
                // do sim work on main thread
                update();
                // wait for a new frame to arrive
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                // copy game state and submit work to render thread
                submitForRendering();

                m_mainFrameNumber++;

#if !THREADED_RENDERING
                m_gfx->renderOneFrame();
#endif
            }
        }
    }

    void Engine::onSceneCameraAdded(SceneCamera* camera)
    {
        glm::uvec2 renderResoltuion = camera->getRenderResolution();

        // create a new SceneView for this new camera
        s_instance->enqueueFrameGfxTask([renderResoltuion](Frame& frame) {
            // todo: deal with memory ownership here
            s_instance->m_views.push_back(new SceneView(renderResoltuion));
        });
    }

    // todo: implement this
    void Engine::onSceneCameraRemoved(SceneCamera* camera)
    {
        // remove the SceneView for this camera
    }

    // todo: is there a way to simplify this ...?
    void Engine::onStaticMeshInstanceAdded(StaticMeshInstance* staticMeshInstance)
    {
        if (staticMeshInstance != nullptr)
        {
            const std::string& instanceKey = staticMeshInstance->getInstanceKey();
            auto entry = m_staticSubMeshInstanceMap.find(instanceKey);
            if (entry != m_staticSubMeshInstanceMap.end())
            {
                UNEXPECTED_FATAL_ERROR();
            }

            i32 instanceID = staticMeshInstance->getInstanceID();
            glm::mat4 localToWorldMatrix = staticMeshInstance->getLocalToWorldMatrix();
            StaticMesh& mesh = *staticMeshInstance->getParentMesh();

            std::vector<u32>& subMeshInstanceIndices = m_staticSubMeshInstanceMap[instanceKey];

            for (u32 i = 0; i < mesh.numSubMeshes(); ++i)
            {
                u32 slot = -1;

                if (!m_emptyStaticSubMeshInstanceSlots.empty())
                {
                    u32 emptySlot = m_emptyStaticSubMeshInstanceSlots.front();
                    slot = emptySlot;
                    m_emptyStaticSubMeshInstanceSlots.pop();
                    m_transformCache[emptySlot] = localToWorldMatrix;

                    mesh[i]->addListener([this, emptySlot, instanceID, instanceKey, localToWorldMatrix](StaticSubMesh* sm) {
                        enqueueFrameGfxTask([this, sm, emptySlot, instanceKey, localToWorldMatrix](Frame& frame) {
                            // this function needs to be run on the rendering thread
                            StaticSubMeshInstance instance = { };
                            instance.staticMeshInstanceKey = instanceKey;
                            instance.subMesh = GfxStaticSubMesh::create(sm->getGeometry());
                            instance.localToWorldMatrix = localToWorldMatrix;
                            m_sceneRenderThread->m_staticSubMeshInstances[emptySlot] = instance;
                        });
                    });
                }
                else
                {
                    m_transformCache.push_back(localToWorldMatrix);
                    u32 newSlot = static_cast<u32>(m_transformCache.size()) - 1;
                    slot = newSlot;
                    mesh[i]->addListener([this, instanceID, instanceKey, localToWorldMatrix](StaticSubMesh* sm) {
                        enqueueFrameGfxTask([this, sm, instanceKey, localToWorldMatrix](Frame& frame) {
                            // this function needs to be run on the rendering thread
                            StaticSubMeshInstance instance = { };
                            instance.staticMeshInstanceKey = instanceKey;
                            instance.subMesh = GfxStaticSubMesh::create(sm->getGeometry());
                            instance.localToWorldMatrix = localToWorldMatrix;
                            m_sceneRenderThread->m_staticSubMeshInstances.push_back(instance);
                        });
                    });
                }

                subMeshInstanceIndices.push_back(slot);
            }
        }
    }

    void Engine::onStaticMeshInstanceRemoved(StaticMeshInstance* staticMeshInstance)
    {
        NOT_IMPLEMENTED_ERROR()
    }

    void Engine::onStaticMeshInstanceTransformUpdated(StaticMeshInstance* staticMeshInstance)
    {
        const auto& entry = m_staticSubMeshInstanceMap.find(staticMeshInstance->getInstanceKey());
        if (entry != m_staticSubMeshInstanceMap.end())
        {
            for (u32 smInstanceIndex : entry->second)
            {
               m_transformCache[smInstanceIndex] = staticMeshInstance->getLocalToWorldMatrix();
            }
        }
    }

    void Engine::enqueueFrameGfxTask(const FrameGfxTask& task)
    {
        m_frameGfxTaskQueue.push(task);
    }
}
