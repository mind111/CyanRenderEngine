#pragma once

#include "Allocator.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "LightRenderable.h"

namespace Cyan
{
    struct ILightComponent : public Component
    {
        /* Component interface */
        virtual const char* getTag() override { return  "ILightComponent"; }

        // todo: abstract away what type of allocator is used.
        /* note:
        * buildRenderableLight(IAllocator* allocator) should not care about what kind of allocator is used and how the memory is allocated. It's 
        * the owner of `this` ILightRenderable object's job to correctly handle releasing resources. For example, the owner of this ILightRenderable instance
        * should be responsible for invoking ~ILightRenderable explicitly if this is constructed using placement new operator.
        */
        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) = 0;
    };

    struct DirectionalLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void upload() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "DirectionalLightComponent"; }

        /* LightComponent interface */
        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) override 
        { 
            return allocator.alloc<DirectionalLightRenderable>(directionalLight);
        }

        DirectionalLightComponent() { }
        DirectionalLightComponent(const glm::vec3 direction, const glm::vec4& colorAndIntensity, bool bCastShadow)
            : directionalLight(direction, colorAndIntensity, bCastShadow)
        { }

        const glm::vec3& getDirection() { return directionalLight.direction; }
        const glm::vec4& getColorAndIntensity() { return directionalLight.colorAndIntensity; }

        void setDirection(const glm::vec3& inDirection) { directionalLight.direction = inDirection; }
        void setColorAndIntensity(const glm::vec4& inColorAndIntensity) { directionalLight.colorAndIntensity = inColorAndIntensity; }

    private:
        DirectionalLight directionalLight = DirectionalLight();
    };

    struct SkyLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void upload() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "SkyLightComponent"; }

        /* LightComponent interface */
        virtual ILightRenderable* buildRenderableLight(LinearAllocator& allocator) override 
        { 
            return nullptr;
        }
    private:

    };

    struct PointLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void upload() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "PointLightComponent"; }

        /* LightComponent interface */
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
