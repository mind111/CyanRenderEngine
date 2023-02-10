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
#if 0 
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
        Texture2DBindless* albedoMap = nullptr;
        Texture2DBindless* normalMap = nullptr;
        Texture2DBindless* metallicRoughnessMap = nullptr;
        Texture2DBindless* occlusionMap = nullptr;
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
#else
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

        std::string name;
        glm::vec4 albedo = glm::vec4(.8f, .8f, .8f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 0.f;
        u32 flag;
    };

    // material implementation using bindless texture
    struct MaterialBindless : public Material
    {
        using TextureHandle = u64;

        struct GpuData
        {
            TextureHandle albedoMap;
            TextureHandle normalMap;
            TextureHandle metallicRoughnessMap;
            TextureHandle emissiveMap;
            TextureHandle occlusionMap;
            TextureHandle padding;
            glm::vec4 albedo;
            f32 metallic;
            f32 roughness;
            f32 emissive;
            u32 flag;
        };

        MaterialBindless(const char* inName)
            : Material(inName)
        {
        }

        GpuData buildGpuData();

        Texture2DBindless* albedoMap = nullptr;
        Texture2DBindless* normalMap = nullptr;
        Texture2DBindless* metallicRoughnessMap = nullptr;
        Texture2DBindless* occlusionMap = nullptr;
        Texture2DBindless* emissiveMap = nullptr;
    };

    struct MaterialTextureAtlas : public Material
    {
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

        GpuData buildGpuData()
        {
        }

        SubtextureDesc albedoMap = { };
        SubtextureDesc normalMap = { };
        SubtextureDesc metallicRoughnessMap = { };
        SubtextureDesc occlusionMap = { };
        SubtextureDesc emissiveMap = { };
    };
#endif
};