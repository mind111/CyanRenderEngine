#pragma once
#include <vector>
#include <queue>

#include <glm/glm.hpp>

#include "Transform.h"
#include "Component.h"

namespace Cyan
{
    class SceneComponent : public Component
    {
    public:
        SceneComponent(Entity* owner, const char* name, const Transform& localTransform);
        ~SceneComponent() { }

        void setParent(SceneComponent* parent);
        void attachToParent(SceneComponent* parent);
        void attachChild(SceneComponent* child);
        void onAttached();
        void detachFromParent();
        void onDetached();
        /**
         * This method assumes that both this SceneComponent's parent's localToWorld transform and
           this SceneComponent's local transform is finalized for the current frame
         */
        void finalizeAndUpdateTransform();
        virtual void onTransformUpdated() { }

        const Transform& getLocalTransform() { return m_local; }
        const Transform& getLocalToWorldTransform() { return m_localToWorld; }
        void setLocalTransform(const Transform& localTransform);
        void setLocalToWorldTransform(const Transform& localToWorldTransform);
    protected:
        SceneComponent* m_parent = nullptr;
        std::vector<SceneComponent*> m_children;
        Transform m_local;
        Transform m_localToWorld;
    };
}
