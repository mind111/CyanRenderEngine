#pragma once
#include <memory>

#ifdef ENGINE_EXPORTS
    #define ENGINE_API __declspec(dllexport)
#else
    #define ENGINE_API __declspec(dllimport)
#endif

namespace Cyan 
{
    class ENGINE_API Engine
    {
    public:
        ~Engine() { }

        static Engine* get();
        void initialize();
        void deinitialize();
        void update();
    private:
        Engine(); // hiding constructor to prohibit direct construction
    };
}
