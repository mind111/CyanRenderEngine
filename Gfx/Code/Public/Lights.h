#pragma once

#include <glm/glm.hpp>

#include "Gfx.h"
#include "ShadowMaps.h"

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
    };

    struct IrradianceCubemap
    {
        IrradianceCubemap(i32 inResolution);
        ~IrradianceCubemap() = default;

        void buildFromCubemap(GHTextureCube* srcCubemap);

        i32 resolution = 64;
        std::unique_ptr<GHTextureCube> cubemap = nullptr;
    };

    struct ReflectionCubemap
    {
        ReflectionCubemap(i32 inResolution);
        ~ReflectionCubemap() = default;

        void buildFromCubemap(GHTextureCube* srcCubemap);

        i32 resolution = 1024;
        std::unique_ptr<GHTextureCube> cubemap = nullptr;
        static std::unique_ptr<GHTexture2D> BRDFLUT;
    private:
        static void buildBRDFLUT();
    };

    class GFX_API SkyLight
    {
    public:
        SkyLight();
        SkyLight(i32 irradianceResolution, i32 reflectionResolution);
        ~SkyLight();

        void buildFromHDRI(GHTexture2D* HDRITex);

        IrradianceCubemap* getIrradianceCubemap() 
        { 
            if (m_irradianceCubemap != nullptr)
            {
                return m_irradianceCubemap.get();
            }
            return nullptr;
        }

        ReflectionCubemap* getReflectionCubemap()
        {
            if (m_reflectionCubemap != nullptr)
            {
                return m_reflectionCubemap.get();
            }
            return nullptr;
        }

    private:
        void buildFromCubemap(GHTextureCube* texCube);

        std::unique_ptr<GHTextureCube> m_cubemap = nullptr;
        std::unique_ptr<IrradianceCubemap> m_irradianceCubemap = nullptr;
        std::unique_ptr<ReflectionCubemap> m_reflectionCubemap = nullptr;
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
