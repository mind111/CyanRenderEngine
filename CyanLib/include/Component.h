#pragma once

#include <string>
#include <vector>

namespace Cyan
{
#if 0
    struct Entity;

    struct Component
    {
        Component(Entity* inOwner, const char* inName);
        Component(Entity* inOwner, Component* inParent, const char* inName);

        virtual void update() { }
        virtual void render() { }
        virtual const char* getTag() { return "Component"; }
        virtual Component* find(const char* inName) { return nullptr; }

        virtual void attach(Component* child);
        virtual void attachTo(Component* parent);
        virtual Component* detach(const char* inName);

        std::string name;
        Entity* owner = nullptr;
        Component* parent = nullptr;
        std::vector<Component*> children;
    };
#else
    class World;
    class Entity;

    class Component
    {
    public:
        Component(Entity* owner, const char* m_name) 
            : m_owner(owner), m_name(m_name)
        { }
        ~Component() { }

        virtual void update() { }

        World* getWorld();

    protected:
        Entity* m_owner = nullptr;
        std::string m_name;
    };
#endif
}
