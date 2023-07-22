#include "Scene.h"
#include "StaticMesh.h"
#include "GfxContext.h"

namespace Cyan
{
    Scene::Scene()
    {

    }

    Scene::~Scene()
    {

    }

    void Scene::addStaticMeshInstance(StaticMeshInstance* staticMeshInstance)
    {
        if (staticMeshInstance != nullptr)
        {
            i32 instanceID = staticMeshInstance->getInstanceID();
            StaticMesh& mesh = *staticMeshInstance->getParentMesh();
            for (i32 i = 0; i < mesh.numSubMeshes(); ++i)
            {
                mesh[i]->addListener([this, instanceID, staticMeshInstance](StaticSubMesh* sm) {
                    // this function needs to be run on the rendering thread
                    StaticSubMeshInstance instance = { };
                    instance.subMesh = GfxContext::get()->createGfxStaticSubMesh(sm->getGeometry());
                    instance.localToWorldMatrix = staticMeshInstance->getLocalToWorldMatrix();
                    m_staticSubMeshInstances.push_back(instance);
                });
            }
        }
    }

    void Scene::removeStaticMeshInstance(StaticMeshInstance* staticMeshInstance)
    {
    }
}

