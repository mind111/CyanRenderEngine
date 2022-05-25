#pragma once

#include "LightProbe.h"
#include "ArHosekSkyModel.h"

namespace Cyan
{
    enum SkyboxConfig
    {
        kUseHDRI,
        kUseProcedural
    };

    // todo: decouple skylight from skybox, skylight can be a stand-alone thing
    struct Skybox
    {
        Skybox(const char* srcImagePath, const glm::vec2& resolution, const SkyboxConfig& cfg);
        ~Skybox() { }
        void initialize();
        /* brief:
        * Build a sky dome used for rendering.
        */
        void build();
        /* brief:
        * Build a SkyLight from a sky dome.
        */
        void buildSkyLight();
        void render();

        Texture* getDiffueTexture()
        {
            return m_diffuseProbe->m_convolvedIrradianceTexture;
        }

        Texture* getSpecularTexture()
        {
            return m_specularProbe->m_convolvedReflectionTexture;
        }

        static Shader* s_CubemapSkyShader;
        static Shader* s_proceduralSkyShader;

        SkyboxConfig     m_cfg;
        Texture* m_srcHDRITexture;
        Texture* m_srcCubemapTexture;
        IrradianceProbe* m_diffuseProbe;
        ReflectionProbe* m_specularProbe;
    };

    struct HosekSkyDome
    {
        ArHosekSkyModelState* stateR;
        ArHosekSkyModelState* stateG;
        ArHosekSkyModelState* stateB;

        HosekSkyDome(const glm::vec3& sunDir, const glm::vec3& groundAlbedo)
            : stateR(nullptr), stateG(nullptr), stateB(nullptr)
        {
            // in radians
            f32 solarElevation = acos(glm::dot(sunDir, glm::vec3(0.f, 1.f, 0.f)));
            auto stateR = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.r, solarElevation);
            auto stateG = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.g, solarElevation);
            auto stateB = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.b, solarElevation);
        }

        glm::vec3 sample(const glm::vec3& dir)
        {

        }
    };
}
