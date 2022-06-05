#pragma once

#include "Common.h"
#include "DirectionalLight.h"
#include "Shader.h"

struct Scene;

namespace Cyan
{
    struct Renderer;

    struct ILightRenderable
    {
        virtual void renderShadow(const Scene& scene, Renderer& renderer) { }
        virtual void setShaderParameters(Shader* shader) = 0;
    };

    /**
    * Renderable DirectionalLight
    */
    struct DirectionalLightRenderable : public ILightRenderable
    {
        /* ILightRenderable interface */
        virtual void renderShadow(const Scene& scene, Renderer& renderer) override
        {
            shadowmapPtr->render(scene, renderer);
        }

        virtual void setShaderParameters(Shader* shader) override
        {
            shader->setUniform("sceneLights.directionalLight.direction", direction);
            shader->setUniform("sceneLights.directionalLight.colorAndIntensity", colorAndIntensity);
            shadowmapPtr->setShaderParameters(shader);
        }

        DirectionalLightRenderable(const DirectionalLight& inDirectionalLight) 
            : direction(glm::vec4(inDirectionalLight.direction, 0.f)), colorAndIntensity(inDirectionalLight.colorAndIntensity), bCastShadow(inDirectionalLight.bCastShadow)
        { 
            // create shadowmap
            if (bCastShadow)
            {
                switch (inDirectionalLight.implemenation)
                {
                case DirectionalLight::Implementation::kCSM:
                    shadowmapPtr = std::make_unique<CascadedShadowmap>(inDirectionalLight);
                    break;
                case DirectionalLight::Implementation::kVSM:
                    break;
                default:
                    break;
                }
            }
        }

    private:
        glm::vec4 direction;
        glm::vec4 colorAndIntensity;
        std::unique_ptr<IDirectionalShadowmap> shadowmapPtr = nullptr;
        bool bCastShadow = false;
    };
}
