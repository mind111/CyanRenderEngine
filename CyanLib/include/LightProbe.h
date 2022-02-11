#pragma once

#include "Texture.h"
#include "Entity.h"

struct DistantLightProbe
{
    Cyan::Texture* m_baseCubeMap;
    Cyan::Texture* m_diffuse;
    Cyan::Texture* m_specular;
    Cyan::Texture* m_brdfIntegral;
};

struct Scene;

namespace Cyan
{
    struct LightProbe
    {
        LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution);
        ~LightProbe() { }
        virtual void initialize();
        virtual void captureScene();
        virtual void debugRender();

        Scene*               m_scene;
        glm::vec3            m_position;
        glm::vec2            m_resolution;
        Texture*             m_sceneCapture;
        MeshInstance*        m_debugSphereMesh;
        MaterialInstance*    m_debugRenderMatl;
    };

    struct IrradianceProbe : public LightProbe
    {
        IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceResolution);
        ~IrradianceProbe() { }
        virtual void debugRender() override;
        void convolve();

        static Shader* m_convolveIrradianceShader;
        static RegularBuffer*      m_rayBuffers;
        static const u32 kNumZenithSlice       = 32u;
        static const u32 kNumAzimuthalSlice    = 32u;
        static const u32 kNumRaysPerHemiSphere = 128u;

        glm::vec2         m_irradianceTextureRes;
        Texture*          m_convolvedIrradianceTexture;
        MaterialInstance* m_convolveIrradianceMatl;
    };

    struct IrradianceVolume
    {
        glm::vec3 m_volumePos;
        glm::vec3 m_volumeDimension;
        glm::vec3 m_probeSpacing;
        glm::vec3 m_lowerLeftCorner;
        std::vector<IrradianceProbe*> m_probes;
    };

    struct ReflectionProbe : public LightProbe
    {
        ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution);
        ~ReflectionProbe() { }
        virtual void debugRender() override;
        void convolve();
        void bake();

        static const u32     kNumMips = 11; 
        static Shader*       s_convolveReflectionShader;
        Texture*             m_convolvedReflectionTexture;
        MaterialInstance*    m_convolveReflectionMatl;
    };

    class LightProbeFactory
    {
    public:
        IrradianceProbe* createIrradianceProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceResolution);
        ReflectionProbe* createReflectionProbe(Scene* scene, const glm::vec3& position, const glm::uvec2& sceneCaptureRes);
    };
}