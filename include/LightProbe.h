#pragma once

#include "Texture.h"
#include "Entity.h"

struct LightProbe
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
        // render the scene from six faces
        Texture* m_radianceMap;
        // convolve the radiance samples to get irradiance from every direction
        Texture* m_irradianceMap;
        Scene* m_scene;
        // MeshInstance* m_sphereMeshInstance;
        MaterialInstance* m_computeIrradianceMatl;
        MaterialInstance* m_renderProbeMatl;
        MeshInstance* m_cubeMeshInstance;

        static Shader* m_computeIrradianceShader;
        static struct RenderTarget* m_radianceRenderTarget;
        static struct RenderTarget* m_irradianceRenderTarget;
        IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene);
        void init();
        void sampleRadiance();
        void computeIrradiance();
        void debugRenderProbe();
    };

    // TODO: fix cornell box to make it double sided
    // TODO: save / load the probe
    // TODO: ray tracing cornell using single light field probe
    // TODO: enable multi-bounce only for cornell or make a dedicated scene to just test cornell box
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
        void resampleToOctMap();
        void sampleRadianceOctMap();
        void sampleRadialDistanceAndNormal(); 
        void debugRenderProbe();
        void save();
    };

    class LightProbeFactory
    {
    public:
        IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3 position);
        LightFieldProbe* createLightFieldProbe(Scene* scene, glm::vec3 position);
        u32 numIrradianceProbe;
        u32 numLightFieldProbe;
    };
}