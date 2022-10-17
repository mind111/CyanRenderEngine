#pragma once
namespace Cyan
{
    class System
    {
    public:
        System() = default;
        virtual ~System() { }
        virtual void initialize() = 0;
        virtual void upload() { }
        virtual void finalize() = 0;
    };
}
