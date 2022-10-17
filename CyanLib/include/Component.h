#pragma once

namespace Cyan
{
    // todo: how to customize a component's behavior? allow different instances of components have different behavior
    // via binding function pointers directly to component or let entity to customize their upload() function, and it's entity's job to 
    // customize the behavior of its component
    struct Component
    {
        virtual void upload() { }
        virtual void render() { }
        virtual const char* getTag() { return "Component"; }
    };
}
