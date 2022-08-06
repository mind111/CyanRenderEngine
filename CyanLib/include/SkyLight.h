#pragma once

#include "Light.h"
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
        std::shared_ptr<IrradianceProbe> irradianceProbe = nullptr;
        std::shared_ptr<ReflectionProbe> reflectionProbe = nullptr;
    };
}
