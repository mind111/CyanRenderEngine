#pragma once

#include "Common.h"
#include "DirectionalLight.h"
#include "Shader.h"

namespace Cyan
{
    struct Scene;
    struct Renderer;

    struct ILightRenderable
    {
        virtual ~ILightRenderable() { }

        virtual void renderShadowMaps(const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) { }
        virtual void setShaderParameters(Shader* shader) = 0;
        virtual ILightRenderable* clone() { return nullptr; }
    };

    /**
    * Renderable DirectionalLight
    */
    struct DirectionalLightRenderable : public ILightRenderable
    {
        /* ILightRenderable interface */
        virtual void renderShadowMaps(const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) override;
        virtual void setShaderParameters(Shader* shader) override
        {
            shader->setUniform("sceneLights.directionalLight.direction", direction);
            shader->setUniform("sceneLights.directionalLight.colorAndIntensity", colorAndIntensity);
            glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -vec4ToVec3(direction), glm::vec3(0.f, 1.f, 0.f));
            shader->setUniform("sceneLights.directionalLight.lightSpaceView", lightSpaceView);
            shadowmapPtr->setShaderParameters(shader, "sceneLights.directionalLight");
        }
        virtual ILightRenderable* clone() {
            return nullptr;
        }

        DirectionalLightRenderable(const DirectionalLight& inDirectionalLight) 
            : direction(glm::vec4(inDirectionalLight.direction, 0.f)), colorAndIntensity(inDirectionalLight.colorAndIntensity), bCastShadow(inDirectionalLight.bCastShadow)
        { 
            // create shadowmap
            if (bCastShadow)
            {
                switch (inDirectionalLight.implemenation)
                {
                case DirectionalLight::Implementation::kCSM_Basic:
                    shadowmapPtr = std::make_unique<CascadedShadowmap>(inDirectionalLight);
                    break;
                case DirectionalLight::Implementation::kBasic:
                    break;
                case DirectionalLight::Implementation::kVarianceShadowmap:
                    break;
                default:
                    break;
                }
            }
        }

        ~DirectionalLightRenderable()
        {

        }

    private:
        glm::vec4 direction;
        glm::vec4 colorAndIntensity;
        std::unique_ptr<IDirectionalShadowmap> shadowmapPtr = nullptr;
        bool bCastShadow = false;
    };
}
