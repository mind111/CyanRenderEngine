#pragma once

#include "Allocator.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "LightRenderable.h"

namespace Cyan
{
    struct ILightComponent : public Component
    {
        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) = 0;
    };

    struct DirectionalLightComponent : public ILightComponent
    {
        virtual void update() override { }
        virtual void render() override { }

        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) override 
        { 
            return allocator.alloc<DirectionalLightRenderable>(directionalLight);
        }

        void getDirection() { }
        void getColor() { }

        void setDirection() { }
        void setColor() { }

        static const char* tag;

    private:
        DirectionalLight directionalLight;
    };

    struct PointLightComponent : public ILightComponent
    {
        virtual void update() override { }
        virtual void render() override { }

        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) override 
        { 
            return nullptr;
        }

        void getPosition() { }
        void getColor() { }

        void setPosition() { }
        void setColor() { }
    private:
        PointLight pointLight;
    };
}
