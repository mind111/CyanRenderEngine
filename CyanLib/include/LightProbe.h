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
    // TODO: group shader and material instance, technically don't need to store shader and matl instance at the same time.
    struct IrradianceProbe : public Entity
    {
        IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene);
        void sampleRadiance();
        void computeIrradiance();
        void debugRenderProbe();

        static Shader* m_computeIrradianceShader;
        static struct RenderTarget* m_radianceRenderTarget;
        static struct RenderTarget* m_irradianceRenderTarget;
        static MeshInstance* m_cubeMeshInstance;
        static RegularBuffer*      m_rayBuffers;
        static const u32 kNumZenithSlice       = 32u;
        static const u32 kNumAzimuthalSlice    = 32u;
        static const u32 kNumRaysPerHemiSphere = 128u;

        // render the scene from six faces
        Texture* m_radianceMap;
        // convolve the radiance samples to get irradiance from every direction
        Texture* m_irradianceMap;
        Scene* m_scene;
        // MeshInstance* m_sphereMeshInstance;
        MaterialInstance* m_computeIrradianceMatl;
        MaterialInstance* m_renderProbeMatl;
    };

    struct IrradianceVolume
    {
        glm::vec3 m_volumePos;
        glm::vec3 m_volumeDimension;
        glm::vec3 m_probeSpacing;
        glm::vec3 m_lowerLeftCorner;
        std::vector<IrradianceProbe*> m_probes;
    };

    struct ReflectionProbe : public Entity
    {
        ReflectionProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene);
        void sampleSceneRadiance();
        void convolve();
        void bake();

        static const u32     kNumMips = 11; 
        static RenderTarget* m_renderTarget;
        static RenderTarget* m_prefilterRts[kNumMips];
        static Shader*       m_convolveSpecShader;
        Scene* m_scene;
        Texture* m_radianceMap;
        Texture* m_prefilteredProbe;
        MaterialInstance* m_renderProbeMatl;
        MaterialInstance* m_convolveSpecMatl;
    };

    class LightProbeFactory
    {
    public:
        IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3 position);
        ReflectionProbe* createReflectionProbe(Scene* scene, glm::vec3 position);
        u32 numIrradianceProbe;
        u32 numLightFieldProbe;
    };
}