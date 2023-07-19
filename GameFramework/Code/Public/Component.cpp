#include "Common.h"
#include "Entity.h"
#include "Component.h"

namespace Cyan
{
    World* Component::getWorld()
    {
        return m_owner->getWorld();
    }

    Entity* Component::getOwner()
    {
        return m_owner;
    }

    void Component::setOwner(Entity* owner)
    {
        m_owner = owner;
    }
}