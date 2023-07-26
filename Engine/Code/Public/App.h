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
        friend class Engine;

        App(const char* name, int windowWidth, int windowHeight);
        virtual ~App();

        void initialize(World* world);
        void deinitialize();
        virtual void update(World* world);
        virtual void render();

    protected:
        virtual void customInitialize(World* world);
        virtual void customDeinitialize();

        std::string m_name;
        int m_windowWidth, m_windowHeight;
    };
}
