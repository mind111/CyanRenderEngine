#pragma once

#include "App.h"

namespace Cyan
{
    class ParallaxOcclusionMapping : public App
    {
    public:
        ParallaxOcclusionMapping(u32 windowWidth, u32 windowHeight);
        virtual ~ParallaxOcclusionMapping();

        virtual void update(World* world) override;
        virtual void render() override;
    protected:
        virtual void customInitialize(World* world) override;
    };
}
