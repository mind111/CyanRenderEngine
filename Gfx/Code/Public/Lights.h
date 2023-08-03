#pragma once

#include <glm.hpp>

#include "Gfx.h"
#include "ShadowMaps.h"
// #include "LightProbe.h"

namespace Cyan
{
    class GFX_API Light 
    {
    public:
        Light(const glm::vec3& color, f32 intensity);
        virtual ~Light();

        glm::vec3 m_color;
        f32 m_intensity;
    };

    class GFX_API DirectionalLight : public Light
    {
    public:
        DirectionalLight(const glm::vec3& color, f32 intensity, const glm::vec3& direction);
        ~DirectionalLight() { }

        static constexpr glm::vec3 defaultDirection = glm::vec3(0.f);

        glm::vec3 m_direction;
        // std::unique_ptr<CascadedShadowMap> m_csm = nullptr;
    };

#if 0
    class SkyLightComponent;
    class SkyLight : public Light
    {
    public:
        SkyLight(SkyLightComponent* skyLightComponent);
        ~SkyLight();

        void buildFromHDRI(Texture2D* HDRI);
        void buildFromScene(Scene* scene);

        static constexpr u32 cubemapCaptureResolution = 1024u;
        static constexpr u32 irradianceResolution = 32u;
        static constexpr u32 reflectionResolution = 1024u;

        SkyLightComponent* m_skyLightComponent = nullptr;
        std::unique_ptr<GfxTextureCube> m_cubemap = nullptr;
        std::unique_ptr<IrradianceProbe> m_irradianceProbe = nullptr;
        std::unique_ptr<ReflectionProbe> m_reflectionProbe = nullptr;
    };

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
