#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"

namespace Cyan
{
    StaticMeshEntity::StaticMeshEntity(World* world, const char* name, const Transform& local, const Transform& localToWorld, Entity* parent, std::shared_ptr<StaticMesh> mesh)
        : Entity(world, name, local, localToWorld, parent)
    {
        m_staticMeshComponent = std::make_shared<StaticMeshComponent>("StaticMeshComponent", Transform(), mesh);
        m_rootSceneComponent->attachChild(m_staticMeshComponent);
    }

    StaticMeshEntity::~StaticMeshEntity() { }

    StaticMeshComponent* StaticMeshEntity::getStaticMeshComponent()
    {
        return m_staticMeshComponent.get();
    }

    void StaticMeshEntity::setMaterial(std::shared_ptr<Material> material, i32 index)
    {
        m_staticMeshComponent->setMaterial(material, index);
    }
}