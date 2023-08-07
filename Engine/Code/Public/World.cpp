#include "World.h"
#include "AssetManager.h"
#include "StaticMesh.h"
#include "Engine.h"
#include "GfxModule.h"
#include "SceneCamera.h"
#include "Scene.h"

namespace Cyan
{
    World::World(const char* name)
        : m_name(name)
    {
        assert(m_root == nullptr);
        m_root = createEntity<Entity>(m_name.c_str(), Transform());
        m_sceneRenderThread = std::make_unique<Scene>();
    }

    World::~World()
    {

    }

    void World::update()
    {
        // update each entity
        std::queue<Entity*> q;
        q.push(m_root.get());
        while (!q.empty())
        {
            auto e = q.front();
            e->update();
            q.pop();

            for (auto child : e->m_children)
            {
                q.push(child.get());
            }
        }

        // simulate physics

        // finalize all the transforms
        m_root->getRootSceneComponent()->finalizeAndUpdateTransform();
    }

    void World::import(const char* filename)
    {
        AssetManager::import(this, filename);
    }

    void World::addSceneCamera(SceneCamera* camera)
    {
        m_cameras.push_back(camera);

        // create a new SceneView for this new camera
        glm::uvec2 renderResoltuion = camera->getRenderResolution();
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "AddSceneView",
            [this, renderResoltuion](Frame& frame) {
                // todo: deal with memory ownership here
                m_views.push_back(new SceneView(renderResoltuion));
            }
        );
    }

    void World::removeSceneCamera(SceneCamera* sceneCamera)
    {
        bool bFound = false;
        i32 foundAtIndex = -1;
        // doing a simple linear search for now since there shouldn't be that many viewing cameras anyway
        for (i32 i = 0; i < m_cameras.size(); ++i)
        {
            if (m_cameras[i] == sceneCamera)
            {
                bFound = true;
                foundAtIndex = i;
                m_cameras.erase(m_cameras.begin() + i);
                break;
            }
        }
        assert(bFound);

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "RemoveSceneView",
            [this, foundAtIndex](Frame& frame) {
                // todo: deal with memory ownership here
                m_views.erase(m_views.begin() + foundAtIndex);
            }
        );
    }
}