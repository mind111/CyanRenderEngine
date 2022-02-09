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
        void sampleRadiance();
        void prefilter();
        void bake();

        static const u32     kNumMips = 11; 
        static RenderTarget* m_renderTarget;
        static RenderTarget* m_prefilterRts[kNumMips];
        static Shader*       m_prefilterShader;
        Scene* m_scene;
        Texture* m_radianceMap;
        Texture* m_prefilteredProbe;
        MaterialInstance* m_renderProbeMatl;
        MaterialInstance* m_prefilterMatl;
    };

    struct LightFieldProbe : public Entity
    {
        Texture* m_radiance;
        Texture* m_radianceOct;
        Texture* m_normal;
        Texture* m_normalOct;
        Texture* m_radialDistance;
        Texture* m_distanceOct;
        MaterialInstance* m_renderProbeMatl;
        MaterialInstance* m_octProjMatl;
        static Shader* m_octProjectionShader;
        static RenderTarget* m_cubemapRenderTarget;
        static RenderTarget* m_octMapRenderTarget;

        Scene* m_scene;

        Line2D* m_octMapDebugLines[6];

        LightFieldProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene);
        void sampleScene();
        void debugRenderProbe();
        void save();
    };

    struct LightFieldProbeVolume
    {
        LightFieldProbeVolume(glm::vec3& pos, glm::vec3& dimension, glm::vec3& spacing);
        ~LightFieldProbeVolume() { }
        void sampleScene();
        void packProbeTextures();
        glm::vec3 m_volumePos;
        glm::vec3 m_volumeDimension;
        glm::vec3 m_probeSpacing;
        glm::vec3 m_lowerLeftCorner;
        std::vector<LightFieldProbe*> m_probes;
        Texture* m_octRadianceGrid;
        Texture* m_octNormalGrid;
        Texture* m_octRadialDepthGrid;
    };

    class LightProbeFactory
    {
    public:
        IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3 position);
        ReflectionProbe* createReflectionProbe(Scene* scene, glm::vec3 position);
        LightFieldProbe* createLightFieldProbe(Scene* scene, glm::vec3 position);
        LightFieldProbeVolume* createLightFieldProbeVolume(Scene* scene, glm::vec3& position, glm::vec3& dimension, glm::vec3& spacing);
        u32 numIrradianceProbe;
        u32 numLightFieldProbe;
    };
}