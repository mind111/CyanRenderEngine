#pragma once

#include <memory>
#include <queue>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Geometry.h"
#include "Component.h"
#include "SceneComponent.h"

#define kEntityNameMaxLen 128u

void transformRayToObjectSpace(glm::vec3& ro, glm::vec3& rd, glm::mat4& transform);
f32  transformHitFromObjectToWorldSpace(glm::vec3& objectSpaceHit, glm::mat4& transform, const glm::vec3& roWorldSpace, const glm::vec3& rdWorldSpace);

#define EntityFlag_kStatic 1 << (u32)Cyan::Entity::Mobility::kStatic 
#define EntityFlag_kDynamic 1 << (u32)Cyan::Entity::Mobility::kDynamic
#define EntityFlag_kVisible 1 << (u32)Cyan::Entity::Visibility::kVisible
#define EntityFlag_kCastShadow 1 << (u32)Cyan::Entity::Lighting::kCastShadow

namespace Cyan
{
    class World;

    class Entity
    {
    public:
        friend class World;

        Entity(World* world, const char* name, const Transform& local);
        ~Entity() { }

        virtual void update();

        bool isRootEntity();
        void attachChild(Entity* childEntity);
        void onAttached();
        void detach();
        void onDetached();
        void setParent(Entity* parent);
        virtual void attachSceneComponent(const char* parentComponentName, std::shared_ptr<SceneComponent> componentToAttach);
        virtual void addComponent(std::shared_ptr<Component> component);
        SceneComponent* findSceneComponent(const char* componentName);

        const std::string& getName() { return m_name; }
        World* getWorld() { return m_world; }
        Entity* getParent() { return m_parent; }
        Entity* getChild(i32 index) { return m_children[index]; }
        SceneComponent* getRootSceneComponent() { return m_rootSceneComponent.get(); }
        Component* getComponent(i32 index) { return m_components[index].get(); }
        i32 numChildren() { return m_children.size(); }
        i32 numComponents() { return m_components.size(); }

        void forEachChild(const std::function<void(Entity*)>& func) 
        { 
            for (auto e : m_children)
            {
                func(e);
            }
        }

        void forEachSceneComponent(const std::function<void(SceneComponent*)>& func)
        {
            // todo: BFS to traverse the scene components
        }

        void forEachComponent(const std::function<void(Component*)>& func) 
        { 
            for (auto c : m_components)
            {
                func(c.get());
            }
        }

    protected:
        std::string m_name;
        World* m_world = nullptr;
        Entity* m_parent = nullptr;
        std::vector<Entity*> m_children;
        std::shared_ptr<SceneComponent> m_rootSceneComponent = nullptr;
        // SceneComponents owned by this entity
        std::vector<std::shared_ptr<Component>> m_components;
    };
}