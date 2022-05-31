#pragma once

#include "Entity.h"
#include "texture.h"
#include "Component.h"

namespace Cyan
{
    struct Renderer;

    struct DirectionalLight
    {
        glm::vec3 direction;
        glm::vec4 color;
    };

    struct DirectionalLightComponent : public Component
    {
        virtual void update() override { }
        virtual void render() override { }

        void getDirection() { }
        void getColor() { }

        void setDirection() { }
        void setColor() { }

        static const char* tag;

    private:
        DirectionalLight light;
    };

    struct IDirectionalShadowmap
    {
        enum class Quality
        {
            kLow,
            kMedium,
            kHigh
        } quality;

        virtual void render(Renderer* renderer) { }

        IDirectionalShadowmap()
            : quality(Quality::kHigh)
        { }
    };

    struct CSM : public IDirectionalShadowmap
    {
        CSM()
            : IDirectionalShadowmap()
        {

        }
 
        virtual void render(Renderer* renderer) override { }
    };

    struct DirectionalLightRenderable
    {
        std::unique_ptr<IDirectionalShadowmap> shadowmapPtr = nullptr;

        DirectionalLightRenderable(const DirectionalLight& directionalLight) 
            : direction(glm::vec4(directionalLight.direction, 0.f)), color(directionalLight.color)
        { 

        }

    private:
        glm::vec4 direction;
        glm::vec4 color;
    };
}
