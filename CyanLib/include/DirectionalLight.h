#pragma once

#include "Entity.h"
#include "texture.h"
#include "Component.h"
#include "Shadow.h"

namespace Cyan
{
    struct Renderer;

    struct DirectionalLight
    {
        enum class ShadowQuality
        {
            kLow,
            kMedium,
            kHigh,
            kInvalid
        } shadowQuality = ShadowQuality::kHigh;

        enum class Implementation
        {
            kCSM,
            kVSM,
            kCount
        } implemenation = Implementation::kCSM;

        glm::vec3 direction;
        glm::vec4 color;
        bool bCastShadow = false;
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

    struct DirectionalLightRenderable
    {
        std::unique_ptr<IDirectionalShadowmap> shadowmapPtr = nullptr;

        DirectionalLightRenderable(const DirectionalLight& inDirectionalLight) 
            : direction(glm::vec4(inDirectionalLight.direction, 0.f)), color(inDirectionalLight.color), bCastShadow(inDirectionalLight.bCastShadow)
        { 
            // create shadowmap
            if (bCastShadow)
            {
                switch (inDirectionalLight.implemenation)
                {
                case DirectionalLight::Implementation::kCSM:
                    shadowmapPtr = std::make_unique<CSM>(inDirectionalLight);
                    break;
                case DirectionalLight::Implementation::kVSM:
                    break;
                default:
                    break;
                }
            }
        }

    private:
        bool bCastShadow = false;
        glm::vec4 direction;
        glm::vec4 color;
    };
}
