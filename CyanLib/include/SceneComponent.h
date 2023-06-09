#pragma once
#include <vector>
#include <queue>
#include <memory>

#include <glm/glm.hpp>

#include "Transform.h"
#include "Component.h"

namespace Cyan
{
    class SceneComponent : public Component
    {
    public:
        friend class Entity;

        SceneComponent(const char* name, const Transform& localTransform);
        ~SceneComponent() { }

        void setParent(SceneComponent* parent);
        void attachToParent(SceneComponent* parent);
        void attachChild(std::shared_ptr<SceneComponent> child);
        void onAttached();
        void detachFromParent();
        void onDetached();
        /**
         * This method assumes that both this SceneComponent's parent's localToWorld transform and
           this SceneComponent's local transform is finalized for the current frame
         */
        void finalizeAndUpdateTransform();
        virtual void setOwner(Entity* owner) override;
        virtual void onTransformUpdated() { }

        SceneComponent* getParent() { return m_parent; }
        const Transform& getLocalTransform() { return m_local; }
        const Transform& getLocalToWorldTransform() { return m_localToWorld; }
        void setLocalTransform(const Transform& localTransform);
        void setLocalToWorldTransform(const Transform& localToWorldTransform);
    protected:
        SceneComponent* m_parent = nullptr;
        std::vector<std::shared_ptr<SceneComponent>> m_children;
        Transform m_local;
        Transform m_localToWorld;
    };
}
