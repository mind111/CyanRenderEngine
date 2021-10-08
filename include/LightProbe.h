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

        static Shader* m_renderProbeShader;
        static Shader* m_computeIrradianceShader;
        static struct RenderTarget* m_radianceRenderTarget;
        static struct RenderTarget* m_irradianceRenderTarget;
        IrradianceProbe(const char* name, u32 id, glm::vec3& p, Entity* parent, Scene* scene);
        void init();
        void sampleRadiance();
        void computeIrradiance();
        void debugRenderProbe();
    };

    class LightProbeFactory
    {
    public:
        IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3 position);
    };
}