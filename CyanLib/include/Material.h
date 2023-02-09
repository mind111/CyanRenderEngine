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

namespace Cyan 
{
    struct GpuMaterial 
    {
        u64 albedoMap;
        u64 normalMap;
        u64 metallicRoughnessMap;
        u64 occlusionMap;
        glm::vec4 albedo = glm::vec4(.9f, .9f, .9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
        u32 flag = 0u;
    };

    struct Material
    {
        enum class Flags : u32 {
            kHasAlbedoMap            = 1 << 0,
            kHasNormalMap            = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
        };

        void renderUI();
        GpuMaterial buildGpuMaterial();

        std::string name;
        Texture2D* albedoMap = nullptr;
        Texture2D* normalMap = nullptr;
        Texture2D* metallicRoughnessMap = nullptr;
        Texture2D* occlusionMap = nullptr;
        glm::vec4 albedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
    };

    struct GpuMaterialTextureAtlas
    {
        PackedTextureDesc albedoMap;
        PackedTextureDesc normalMap;
        PackedTextureDesc metallicRoughnessMap;
        PackedTextureDesc occlusionMap;
        glm::vec4 albedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
        u32 flag;
    };

    struct MaterialTextureAtlas
    {
        enum class Flags : u32 
        {
            kHasAlbedoMap            = 1 << 0,
            kHasNormalMap            = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
        };

        std::string name;

        PackedTextureDesc albedoMap;
        PackedTextureDesc normalMap;
        PackedTextureDesc metallicRoughnessMap;
        PackedTextureDesc occlusionMap;

        glm::vec4 albedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
    };
};