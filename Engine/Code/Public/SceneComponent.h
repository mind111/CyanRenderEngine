#pragma once

#include "Core.h"
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
        const Transform& getLocalSpaceTransform() { return m_localSpaceTransform; }
        const Transform& getWorldSpaceTransform() { return m_worldSpaceTransform; }
        void setLocalSpaceTransform(const Transform& localTransform);
        void setWorldSpaceTransform(const Transform& localToWorldTransform);
    protected:
        SceneComponent* m_parent = nullptr;
        // todo: don't need to use shared_ptr here
        std::vector<std::shared_ptr<SceneComponent>> m_children;
        Transform m_localSpaceTransform;
        Transform m_worldSpaceTransform;
    };
}
