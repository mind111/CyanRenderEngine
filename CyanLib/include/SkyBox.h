#pragma once

#include "LightProbe.h"
#include "ArHosekSkyModel.h"

namespace Cyan
{
    // todo: decouple skylight from skybox, skylight can be a stand-alone thing
    struct Skybox
    {
        Skybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution);
        Skybox(const char* name, TextureCubeRenderable* srcCubemap);

        ~Skybox() { }

        /** note:
        * this render function assumes that certain scene data such as the global view ssbo is already updated and bound
        */
        void render(RenderTarget* renderTarget);

        static Shader* s_cubemapSkyShader;
        static Shader* s_proceduralSkyShader;

        Texture2DRenderable* m_srcHDRITexture = nullptr;
        TextureCubeRenderable* m_cubemapTexture = nullptr;
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
