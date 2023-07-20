#pragma once

#include "App.h"

namespace Cyan
{
    class ModularApp : public App
    {
    public:
        ModularApp(i32 windowWidth, i32 windowHeight);
        ~ModularApp();
    protected:
        virtual void customInitialize(World* world) override;
    };
}
