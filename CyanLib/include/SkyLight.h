#pragma once

#include "Lights.h"
#include "LightProbe.h"

namespace Cyan
{
    struct SkyLight : public Light
    {
        SkyLight(Scene* scene, const glm::vec4& colorAndIntensity, const char* srcHDRI = nullptr);
        ~SkyLight() { }

        void buildCubemap(Texture2DRenderable* srcEquirectMap, TextureCubeRenderable* dstCubemap);
        void build();

        static u32 numInstances;

        // todo: handle object ownership here
        Texture2DRenderable* srcEquirectTexture = nullptr;
        TextureCubeRenderable* srcCubemap = nullptr;
        std::unique_ptr<IrradianceProbe> irradianceProbe = nullptr;
        std::unique_ptr<ReflectionProbe> reflectionProbe = nullptr;
    };
}
