#pragma once

#include "Common.h"
#include "DirectionalLight.h"
#include "Shader.h"

namespace Cyan
{
    struct ILightRenderable
    {
        virtual void setLightShaderParameter(Shader* shader) = 0;
    };

    /**
    * Renderable DirectionalLight
    */
    struct DirectionalLightRenderable : public ILightRenderable
    {
        DirectionalLightRenderable(const DirectionalLight& inDirectionalLight) 
            : direction(glm::vec4(inDirectionalLight.direction, 0.f)), colorAndIntensity(inDirectionalLight.colorAndIntensity), bCastShadow(inDirectionalLight.bCastShadow)
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

        virtual void setLightShaderParameter(Shader* shader) override
        {
            shader->setUniform("sceneLights.directionalLight.direction", direction);
            shader->setUniform("sceneLights.directionalLight.colorAndIntensity", colorAndIntensity);
            // todo: how to bind shadowmap textures
        }

    private:
        glm::vec4 direction;
        glm::vec4 colorAndIntensity;
        std::unique_ptr<IDirectionalShadowmap> shadowmapPtr = nullptr;
        bool bCastShadow = false;
    };
}
