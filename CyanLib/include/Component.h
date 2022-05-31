#pragma once

namespace Cyan
{
    // todo: how to customize a component's behavior? allow different instances of components have different behavior
    // via binding function pointers directly to component or let entity to customize their update() function, and it's entity's job to 
    // customize the behavior of its component
    struct Component
    {
        virtual void update() { }
        virtual void render() { }
        virtual std::string getTag() { return std::string("Component"); }
    };
}
