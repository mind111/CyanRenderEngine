#pragma once

#include <string>
#include <memory>

#include "Engine.h"

namespace Cyan
{
    class World;

    class ENGINE_API App
    {
    public:
        App(const char* name, int windowWidth, int windowHeight);
        ~App();

        virtual void initalize();
        virtual void deinitialize();
        virtual void update(World* world);
        virtual void render();

    protected:
        std::string m_name;
        int m_windowWidth, m_windowHeight;
    };
}
