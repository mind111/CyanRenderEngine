#pragma once
#include <vector>
#include <queue>

#include "glm.hpp"

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
        /* Component interface */
        virtual const char* getTag() { return "SceneComponent"; }

        /* SceneComponent interface */
        virtual Cyan::MeshInstance* getAttachedMesh() { return nullptr; }

        void attachChild(SceneComponent* inChild);
        void attachIndirectChild(SceneComponent* inChild);
        void onAttachTo(SceneComponent* inParent);
        void removeChild(SceneComponent* inChild);
        void removeIndirectChild(SceneComponent* inChild);
        void onBeingRemoved();

        const Transform& getLocalTransform();
        const Transform& getWorldTransform();
        const glm::mat4& getLocalTransformMatrix();
        const glm::mat4& getWorldTransformMatrix();
        SceneComponent* find(const char* name);

        // owner
        Entity* owner;
        Scene* m_scene;
        // identifier
        std::string name;
        // node hierarchy
        SceneComponent* parent;
        std::vector<SceneComponent*> childs;
        std::vector<SceneComponent*> indirectChilds;
        // transform component
        u32       localTransform;
        u32       globalTransform;
        Transform m_localTransform;
        Transform m_worldTransform;
    };

    struct MeshComponent : public SceneComponent {
        /* SceneComponent interface */
        virtual Cyan::MeshInstance* getAttachedMesh() { return meshInst; }

        void setMaterial(Material* material);
        void setMaterial(Material* material, u32 submeshIndex);

        Cyan::MeshInstance* meshInst = nullptr;
    };
}
