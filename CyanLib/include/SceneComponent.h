#pragma once
#include <vector>
#include <queue>

#include <glm/glm.hpp>

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 
#include "Component.h"

#define kSceneNodeNameMaxLen 128u

namespace Cyan
{
    struct Scene;
    struct Entity;

    struct SceneComponent : public Component
    {
        SceneComponent(Entity* inOwner, const char* inName, const Transform& inTransform);
        SceneComponent(Entity* inOwner, Component* inParent, const char* inName, const Transform& inTransform);

        /* Component interface */
        virtual const char* getTag() { return "SceneComponent"; }

        /* SceneComponent interface */
        virtual Cyan::MeshInstance* getAttachedMesh() { return nullptr; }

#if 0
        void attachChild(SceneComponent* inChild);
        void attachIndirectChild(SceneComponent* inChild);
        void onAttachTo(SceneComponent* inParent);
        void removeChild(SceneComponent* inChild);
        void removeIndirectChild(SceneComponent* inChild);
        void onBeingRemoved();
#endif
        const Transform& getLocalTransform();
        const Transform& getWorldTransform();
        const glm::mat4& getLocalTransformMatrix();
        const glm::mat4& getWorldTransformMatrix();

        Entity* owner;
        Scene* m_scene = nullptr;
        Transform m_localTransform;
        Transform m_worldTransform;

        // std::vector<SceneComponent*> childs;
        // std::vector<SceneComponent*> indirectChilds;
        // transform component
    };

    struct MeshComponent : public SceneComponent 
    {
        MeshComponent(Entity* inOwner, const char* inName, const Transform& inTransform, StaticMesh* inMesh)
            : SceneComponent(inOwner, inName, inTransform)
        {
            meshInst = std::make_unique<MeshInstance>(inMesh);
        }

        /* SceneComponent interface */
        virtual Cyan::MeshInstance* getAttachedMesh() { return meshInst.get(); }

        void setMaterial(Material* material);
        void setMaterial(Material* material, u32 submeshIndex);

        std::unique_ptr<MeshInstance> meshInst = nullptr;
    };
}
