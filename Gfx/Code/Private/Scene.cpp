#include "Scene.h"
#include "GfxContext.h"

namespace Cyan
{
    Scene::Scene()
    {

    }

    Scene::~Scene()
    {

    }

    void Scene::addStaticSubMeshInstance(const StaticSubMeshInstance& staticSubMeshInstance)
    {
        m_staticSubMeshInstances.push_back(staticSubMeshInstance);
    }

    void Scene::removeStaticMeshInstance(StaticMeshInstance* staticMeshInstance)
    {
    }
}

