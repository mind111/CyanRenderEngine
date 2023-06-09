#pragma once

#include <string>
#include <vector>

namespace Cyan
{
    class World;
    class Entity;

    class Component
    {
    public:
        Component(const char* name) 
            : m_name(name)
        { }
        ~Component() { }

        virtual void update() { }

        World* getWorld();
        Entity* getOwner();
        virtual void setOwner(Entity* owner);

    protected:
        Entity* m_owner = nullptr;
        std::string m_name;
    };
}
