#pragma once

#include <string>
#include <vector>

namespace Cyan
{
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
}
