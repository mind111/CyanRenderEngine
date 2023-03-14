#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <array>

#include "Singleton.h"
#include "Shader.h"
#include "Texture.h"
#include "TextureAtlas.h"
#include "Asset.h"
#include "Image.h"
#include "RenderableScene.h"

namespace Cyan 
{
    // todo: Each unique material should map to a unique shader, different materials using the same type shading model basically has some unique vertex shader or pixel shader logic to 
    // calculate material parameters. Same type of shading model only means using the same type of material parameters, and how to calculate the material parameters is defined in the unique
    // shader logic mentioned above. So an unique material can own some custom glsl vertex shader code or pixel shader code for calculating material parameters.

    struct Material
    {
        enum class Flags : u32 
        {
            kHasAlbedoMap            = 1 << 0,
            kHasNormalMap            = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
        };

        Material(const char* inName)
            : name(inName)
        {

        }
        virtual ~Material() { };

        virtual RenderableScene::Material buildGpuMaterial() = 0;
        virtual void setShaderParameters(PixelShader* shader) { };

        std::string name;
        glm::vec4 albedo = glm::vec4(.8f, .8f, .8f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 0.f;
        u32 flag;
    };

    struct MaterialBasic : public Material 
    {
        MaterialBasic(const char* inName)
            : Material(inName)
        {
        }

        virtual RenderableScene::Material buildGpuMaterial() override { };
        virtual void setShaderParameters(PixelShader* shader) override;

        Texture2D* albedoMap = nullptr;
        Texture2D* normalMap = nullptr;
        Texture2D* metallicRoughnessMap = nullptr;
        Texture2D* occlusionMap = nullptr;
        Texture2D* emissiveMap = nullptr;
    };

    // todo: Even if I implement texture atlas later, material
    // material implementation using bindless texture
    struct MaterialBindless : public Material
    {
        MaterialBindless(const char* inName)
            : Material(inName)
        {
        }

        virtual RenderableScene::Material buildGpuMaterial() override;

        Texture2DBindless* albedoMap = nullptr;
        Texture2DBindless* normalMap = nullptr;
        Texture2DBindless* metallicRoughnessMap = nullptr;
        Texture2DBindless* occlusionMap = nullptr;
        Texture2DBindless* emissiveMap = nullptr;
    };

    // todo: GpuData should be identical to MaterialBindless, SubtextureDesc is two i32, basically can be packed into a u64
    struct MaterialTextureAtlas : public Material
    {
#if 0
        struct GpuData
        {
            SubtextureDesc albedoMap;
            SubtextureDesc normalMap;
            SubtextureDesc metallicRoughnessMap;
            SubtextureDesc emissiveMap;
            glm::vec4 albedo;
            f32 metallic;
            f32 roughness;
            f32 emissive;
            u32 flag;
        };
#endif
        MaterialTextureAtlas(const char* inName)
            : Material(inName)
        {

        }

        virtual RenderableScene::Material buildGpuMaterial() override { };

        SubtextureDesc albedoMap = { };
        SubtextureDesc normalMap = { };
        SubtextureDesc metallicRoughnessMap = { };
        SubtextureDesc occlusionMap = { };
        SubtextureDesc emissiveMap = { };
    };
};