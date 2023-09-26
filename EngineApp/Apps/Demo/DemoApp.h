#pragma once

#include "App.h"

namespace Cyan
{
    class DemoApp : public App
    {
    public:
        DemoApp(i32 windowWidth, i32 windowHeight);
        ~DemoApp();
    protected:
        virtual void customInitialize(World* world) override;
    };
}
