#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <array>

#include "Asset.h"
#include "Texture.h"

namespace Cyan 
{
    class PixelShader;

    // todo: Each unique material should map to a unique shader, different materials using the same type shading model basically has some unique vertex shader or pixel shader logic to 
    // calculate material parameters. Same type of shading model only means using the same type of material parameters, and how to calculate the material parameters is defined in the unique
    // shader logic mentioned above. So an unique material can own some custom glsl vertex shader code or pixel shader code for calculating material parameters.

    class Material : public Asset
    {
    public:
        enum class Flags : u32 
        {
            kHasAlbedoMap            = 1 << 0,
            kHasNormalMap            = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
        };

        Material(const char* name)
            : Asset(name)
        {
        }

        virtual ~Material() { };

        static const char* getAssetTypeName() { return "Material"; }

        virtual void setShaderParameters(PixelShader* shader);

        glm::vec4 albedo = glm::vec4(.8f, .8f, .8f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 0.f;
        std::shared_ptr<Texture2D> albedoMap = nullptr;
        std::shared_ptr<Texture2D> normalMap = nullptr;
        std::shared_ptr<Texture2D> metallicRoughnessMap = nullptr;
        std::shared_ptr<Texture2D> occlusionMap = nullptr;
    };
};