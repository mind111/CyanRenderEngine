#pragma once

#include "App.h"

namespace Cyan
{
    class SimpleParticles : public App
    {
    public:
        SimpleParticles(u32 windowWidth, u32 windowHeight);
        virtual ~SimpleParticles();

        virtual void update(World* world) override;
        virtual void render() override;
    protected:
        virtual void customInitialize(World* world) override;
    };
}

