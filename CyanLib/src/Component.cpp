#include "Common.h"
#include "Entity.h"
#include "Component.h"

namespace Cyan
{
#if 0
    Component::Component(Entity* inOwner, const char* inName)
        : name(inName), owner(inOwner), parent(nullptr)
    {
        auto sceneRoot = owner->getRootSceneComponent();
        if (sceneRoot)
        {
            owner->getRootSceneComponent()->attach(this);
        }
    }

    Component::Component(Entity* inOwner, Component* inParent, const char* inName)
        : name(inName), owner(inOwner), parent(inParent)
    {
        parent->attach(this);
    }

    void Component::attach(Component* child)
    {
        children.push_back(child);
        if (child->parent)
        {
            child->parent->detach(child->name.c_str());
        }
        child->parent = this;
    }

    void Component::attachTo(Component* parent)
    {
        parent->attach(this);
    }

    Component* Component::detach(const char* inName)
    {
        Component* found = nullptr;
        if (!children.empty())
        {
            i32 foundAt = -1;
            for (i32 i = 0; i < children.size(); ++i)
            {
                if (children[i]->name == inName)
                {
                    foundAt = i;
                    found = children[i];
                    found->parent = nullptr;
                    break;
                }
            }
            if (foundAt >= 0)
            {
                children.erase(children.begin() + foundAt);
            }
        }
        return found;
    }
#endif

    World* Component::getWorld()
    {
        return m_owner->getWorld();
    }
}