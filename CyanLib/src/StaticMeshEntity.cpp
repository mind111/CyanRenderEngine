#include "StaticMeshEntity.h"
#include "Scene.h"
#include "Material.h"

namespace Cyan
{
    StaticMeshEntity::StaticMeshEntity(Scene* scene, const char* inName, const Transform& t, Mesh* inMesh, Entity* inParent, u32 inProperties)
        : Entity(scene, inName, t, inParent, inProperties)
    {
        meshComponentPtr = std::unique_ptr<MeshComponent>(scene->createMeshComponent(inMesh, Transform{ }));
        attachSceneComponent(meshComponentPtr.get());
        addComponent(meshComponentPtr.get());
    }

    void StaticMeshEntity::renderUIWidgets()
    {
        /**
        * Render a widget to edit meshInstance related properties
        */
    }

    void StaticMeshEntity::setMaterial(IMaterial* material)
    {
        meshComponentPtr->setMaterial(material);
    }

    void StaticMeshEntity::setMaterial(IMaterial* material, u32 index)
    {
        meshComponentPtr->setMaterial(material, index);
    }
}