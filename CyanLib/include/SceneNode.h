#pragma once
#include <vector>
#include <queue>

#include "glm.hpp"

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 

#define kSceneNodeNameMaxLen 128u

namespace Cyan
{
    struct Scene;
    struct Entity;

    struct SceneComponent
    {
        virtual Cyan::MeshInstance* getAttachedMesh() { return nullptr; }

        void setParent(SceneComponent* parent);
        void attachChild(SceneComponent* child);
        void attachIndirectChild(SceneComponent* child);
        void onAttachTo();
        const Transform& getLocalTransform();
        const Transform& getWorldTransform();
        const glm::mat4& getLocalTransformMatrix();
        const glm::mat4& getWorldTransformMatrix();
        SceneComponent* find(const char* name);

        // owner
        Entity* owner;
        Scene* m_scene;
        // identifier
        char m_name[kSceneNodeNameMaxLen];
        // node hierarchy
        SceneComponent* m_parent;
        std::vector<SceneComponent*> m_child;
        std::vector<SceneComponent*> m_indirectChild;
        // transform component
        u32       localTransform;
        u32       globalTransform;
        Transform m_localTransform;
        Transform m_worldTransform;
    };

    struct MeshComponent : public SceneComponent
    {
        /* SceneComponent interface */
        virtual Cyan::MeshInstance* getAttachedMesh() { return meshInst; }

        Cyan::MeshInstance* meshInst = nullptr;
    };
}
