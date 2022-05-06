#pragma once
#include <vector>
#include <queue>

#include "glm.hpp"

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 

#define kSceneNodeNameMaxLen 128u

enum SceneNodeFlag
{
    HasAABB    = 0 << 1,
    CastShadow = 1 << 1,
    NeedUpdate = 2 << 1,
};

struct SceneNode
{
    // owner
    struct Entity* m_owner; 
    struct Scene*  m_scene;
    // identifier
    char m_name[kSceneNodeNameMaxLen];
    // node hierarchy
    SceneNode* m_parent;
    std::vector<SceneNode*> m_child;
    std::vector<SceneNode*> m_indirectChild;
    // transform component
    u32       localTransform;
    u32       globalTransform;
    Transform m_localTransform;
    Transform m_worldTransform;
    // mesh component 
    // Cyan::MeshInstance* m_meshInstance;
    void setParent(SceneNode* parent);
    void attachChild(SceneNode* child);
    void attachIndirectChild(SceneNode* child);
#if 0
    void onAttach();
    void markChanged();
    void markUnchanged();
    void updateWorldTransform();
#endif
    const Transform& getLocalTransform();
    const Transform& getWorldTransform();
    const glm::mat4& getLocalTransformMatrix();
    const glm::mat4& getWorldTransformMatrix();
    void             setLocalTransform(const Transform& mat);
    void             setWorldTransform(const Transform& mat);
    void             setLocalMatrix(const glm::mat4& mat);
    void             setWorldMatrix(const glm::mat4& mat);

    // Cyan::MeshInstance* getAttachedMesh() { return m_meshInstance; }
    SceneNode* find(const char* name);
};

struct MeshNode : public SceneNode
{
    Cyan::MeshInstance* meshInst = nullptr;
};

template <typename Allocator>
class SceneNodeFactory
{
public:
    SceneNodeFactory() = default;

    SceneNode* create()
    {
        SceneNode* node = allocator.alloc();
        return node;
    }
private:
    Allocator allocator;
};

template <typename Allocator>
class MeshNodeFactory
{
public:
    MeshNodeFactory() = default;

    MeshNode* create()
    {
        MeshNode* node = allocator.alloc();
        return node;
    }
private:
    Allocator allocator;
};

namespace Cyan
{
    // todo: are these good ...?
    struct Component
    {
        Component* parent;
        std::vector<Component>* child;
    };

    struct SceneComponent : Component
    {
        u32       localTransform;
        u32       globalTransform;
    };
    
    struct StaticMeshComponent : SceneComponent
    {
        MeshInstance* meshInst;
    };
}