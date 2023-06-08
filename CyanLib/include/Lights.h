#pragma once

#include <glm/glm.hpp>

#include "ShadowMaps.h"
#include "LightProbe.h"

namespace Cyan
{
    class Light 
    {
    public:
        Light(const glm::vec3& color, f32 intensity);
        virtual ~Light();

        glm::vec3 m_color;
        f32 m_intensity;
    };

    class DirectionalLightComponent;
    class DirectionalLight : public Light
    {
    public:
        DirectionalLight(DirectionalLightComponent* directionalLightComponent);
        ~DirectionalLight() { }

        static constexpr glm::vec3 defaultDirection = glm::vec3(0.f);

        DirectionalLightComponent* m_directionalLightComponent = nullptr;
        glm::vec3 m_direction;
        std::unique_ptr<CascadedShadowMap> m_csm = nullptr;
    };

    class SkyLightComponent;
    class SkyLight : public Light
    {
    public:
        SkyLight(SkyLightComponent* skyLightComponent);
        ~SkyLight();

        SkyLightComponent* m_skyLightComponent = nullptr;
        std::unique_ptr<ReflectionProbe> m_reflectionProbe = nullptr;
        std::unique_ptr<IrradianceProbe> m_irradianceProbe = nullptr;
    };

#if 0
    // todo: a light shouldn't need to care about what kind of shadow rendering technique its shadow map is using.
    struct DirectionalLight : public Light 
    {
        DirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow)
            : Light(inColorAndIntensity), direction(glm::normalize(inDirection)), bCastShadow(inCastShadow) 
        { 
            shadowMap = std::make_unique<DirectionalShadowMap>(*this);
        }

        virtual void renderShadowMap(Scene* inScene, Renderer* renderer);
        void setShaderParameters(PixelShader* ps);

        glm::vec3 direction = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
        bool bCastShadow = true;
    private:
        std::shared_ptr<DirectionalShadowMap> shadowMap = nullptr;
    };

#if 0
    struct CSMDirectionalLight : public DirectionalLight 
    {
        CSMDirectionalLight() : DirectionalLight() {
            shadowMap = std::make_shared<CascadedShadowMap>(*this);
        }

        CSMDirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow) 
            : DirectionalLight(inDirection, inColorAndIntensity, inCastShadow) {
            shadowMap = std::make_shared<CascadedShadowMap>(*this);
        }

        virtual void renderShadowMap(RenderableScene& scene, Renderer* renderer) override;

        GpuCSMDirectionalLight buildGpuLight();
    private:
        std::shared_ptr<CascadedShadowMap> shadowMap = nullptr;
    };
#endif

    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB);

    struct PointLight : public Light 
    {
        PointLight() : Light() { }
        PointLight(const glm::vec3& inPosition) 
            : Light(), m_position(inPosition) 
        { 
        }

        glm::vec3 m_position = glm::vec3(0.f);
    };

    struct SkyLight : public Light 
    {
        SkyLight(Scene* scene, const glm::vec4& colorAndIntensity, const char* srcHDRI = nullptr);
        ~SkyLight() { }

        void buildCubemap(Texture2D* srcEquirectMap, GfxTextureCube* dstCubemap);
        void build();

        static u32 numInstances;

        // todo: handle object ownership here
        Texture2D* srcEquirectTexture = nullptr;
        std::unique_ptr<GfxTextureCube> srcCubemap = nullptr;
        std::unique_ptr<IrradianceProbe> irradianceProbe = nullptr;
        std::unique_ptr<ReflectionProbe> reflectionProbe = nullptr;
    };
#endif
}
