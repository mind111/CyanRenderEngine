#pragma once

#include <cassert>

namespace Cyan
{
    template <typename T>
    class Singleton
    {
    public:
        Singleton() 
        {
            assert(singleton == nullptr);
            /** note - @min: This cast is dangerous as this 
            * requires the Singleton<T> to be listed as the first one when
            * being inherited or els "this" pointer won't be pointing at 
            * the start of derived class. How to ensure that this is true?
            */
            singleton = reinterpret_cast<T*>(this);
        }
        virtual ~Singleton() { }

        static T* get()
        {
            // is this performant ...?
            return dynamic_cast<T*>(singleton); 
        }
    protected:
        static T* singleton;
    };
}
