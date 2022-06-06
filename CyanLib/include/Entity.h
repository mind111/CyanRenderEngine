#pragma once

#include <queue>
#include <functional>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "Node.h"
#include "SceneNode.h"
#include "Geometry.h"
#include "Component.h"

#define kEntityNameMaxLen 128u

void transformRayToObjectSpace(glm::vec3& ro, glm::vec3& rd, glm::mat4& transform);
f32  transformHitFromObjectToWorldSpace(glm::vec3& objectSpaceHit, glm::mat4& transform, const glm::vec3& roWorldSpace, const glm::vec3& rdWorldSpace);

#define EntityFlag_kStatic (u32)Entity::Mobility::kStatic << 1
#define EntityFlag_kDynamic (u32)Entity::Mobility::kDynamic << 1
#define EntityFlag_kVisible (u32)Entity::Visibility::kVisible << 1
#define EntityFlag_kCastShadow (u32)Entity::Lighting::kCastShadow << 1

namespace Cyan
{
    /** 
    * every entity has to have a transform component, entity's transform component is represented by
    'rootSceneComponent's transform.
    */
    struct Entity
    {
        enum class Mobility
        {
            kStatic = 0,
            kDynamic,
            kCount
        };

        enum class Visibility
        {
            kVisible = (u32)Mobility::kCount,
            kCount
        };
        
        enum class Lighting
        {
            kCastShadow = (u32)Visibility::kCount,
            kCount
        };

        Entity(Scene* scene, const char* inName, const Transform& t, Entity* inParent = nullptr, u32 inProperties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        virtual ~Entity() { }

        virtual void update() { }

        u32 getProperties() { return properties; }
        SceneComponent* getRootSceneComponent();
        SceneComponent* getSceneComponent(const char* name);
        void attachSceneComponent(SceneComponent* inChild, const char* parent=nullptr);

        // bool castVisibilityRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& modelView);
        // struct RayCastInfo intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& view);

        void attachChild(Entity* child);
        void onAttach();

        // visitor
        void visit(const std::function<void(SceneComponent*)>& func);

        // getters
        i32 getChildIndex(const char* name);
        const Transform& getLocalTransform();
        const Transform& getWorldTransform();
        const glm::vec3& getWorldPosition();

        // setters
        void setParent(Entity* parent);
        void setLocalTransform(const Transform& transform);
        void setMaterial(const char* meshComponentName, i32 submeshIndex, Cyan::IMaterial* matl);

        // e->getComponent<ILightComponent>();
        template <typename ComponentType>
        std::vector<ComponentType*> getComponent()
        {
            std::vector<ComponentType*> foundComponents;
            for (auto component : components)
            {
                // todo: how bad is this dynamic_cast ...?
                if (ComponentType* foundComponent = dynamic_cast<ComponentType*>(component))
                {
                    foundComponents.push_back(foundComponent);
                }
            }
            // avoid costly vector copy when return by value ...?
            return std::move(foundComponents);
        }

        void addComponent(Cyan::Component* component)
        {
            components.push_back(component);
        }

        std::string name;
        Entity* parent;

    private:
        std::vector<Entity*> childs;
        SceneComponent* rootSceneComponent;
        u32 properties;
        std::vector<Cyan::Component*> components;
    };

    struct RayCastInfo
    {
        SceneComponent* m_node;
        i32        smIndex;
        i32        triIndex;
        f32        t;

        RayCastInfo()
        : 
        m_node(nullptr), smIndex(-1), triIndex(-1), t(FLT_MAX)
        {} 

        bool operator<(const RayCastInfo& rhs)
        {
            return t < rhs.t;
        }
    };
}