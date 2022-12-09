#pragma once

#include "glm.hpp"

#include "Component.h"
#include "Entity.h"
#include "Texture.h"
#include "Shadow.h"
#include "GpuLights.h"

namespace Cyan
{
    struct Light {
        Light() { }
        Light(const glm::vec4& inColorAndIntensity) 
        : colorAndIntensity(inColorAndIntensity) {

        }

        glm::vec3 getColor() { return glm::vec3(colorAndIntensity.r, colorAndIntensity.g, colorAndIntensity.b); }
        float getIntensity() { return colorAndIntensity.a; }
        void setColor(const glm::vec4& colorAndIntensity) { }

        glm::vec4 colorAndIntensity = glm::vec4(1.f, 0.7f, 0.9f, 1.f);
    };

    struct DirectionalLight : public Light {
        enum class ShadowMap {
            kBasic = 0,
            kCSM,
            kVSM,
        };

        DirectionalLight() : Light() {
            shadowMap = std::make_unique<DirectionalShadowMap>(*this);
        }

        DirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow)
            : Light(inColorAndIntensity), direction(glm::normalize(inDirection)), bCastShadow(inCastShadow) { 
            shadowMap = std::make_unique<DirectionalShadowMap>(*this);
        }

        virtual void renderShadowMap(SceneRenderable& scene, Renderer* renderer);

        glm::vec3 direction = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
        bool bCastShadow = true;
    private:
        std::shared_ptr<DirectionalShadowMap> shadowMap = nullptr;
    };

    struct CSMDirectionalLight : public DirectionalLight {
        CSMDirectionalLight() : DirectionalLight() {
            shadowMap = std::make_shared<CascadedShadowMap>(*this);
        }

        CSMDirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow) 
            : DirectionalLight(inDirection, inColorAndIntensity, inCastShadow) {
            shadowMap = std::make_shared<CascadedShadowMap>(*this);
        }

        virtual void renderShadowMap(SceneRenderable& scene, Renderer* renderer) override;

        GpuCSMDirectionalLight buildGpuLight();
    private:
        std::shared_ptr<CascadedShadowMap> shadowMap = nullptr;
    };

    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB);

    struct PointLight : public Light {
        PointLight() : Light() { }
        PointLight(const glm::vec3& inPosition) 
            : Light(), position(inPosition) { 
        }

        glm::vec3 position = glm::vec3(0.f);
    };

    struct SkyLight : public Light {
        SkyLight(Scene* scene, const glm::vec4& colorAndIntensity, const char* srcHDRI = nullptr);
        ~SkyLight() { }

        void buildCubemap(Texture2DRenderable* srcEquirectMap, TextureCubeRenderable* dstCubemap);
        void build();

        static u32 numInstances;

        // todo: handle object ownership here
        Texture2DRenderable* srcEquirectTexture = nullptr;
        TextureCubeRenderable* srcCubemap = nullptr;
        std::unique_ptr<IrradianceProbe> irradianceProbe = nullptr;
        std::unique_ptr<ReflectionProbe> reflectionProbe = nullptr;
    };
}
