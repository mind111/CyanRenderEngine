#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

#include "Shader.h"
#include "Texture.h"
#include "Asset.h"
#include "CyanUI.h"

namespace Cyan {
#if 0
    struct IAsset;

    struct MaterialParameter
    {
        enum class Flags
        {
            kHasRoughnessMap         = 1 << 0,
            kHasMetallicMap          = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
            kHasDiffuseMap           = 1 << 4,
            kHasNormalMap            = 1 << 5,
            kUseLightMap             = 1 << 7             
        };

        virtual void renderUI() { };
        virtual void setShaderParameters(Shader* shader) = 0;
        virtual bool isLit() = 0;
    };

    struct ConstantColor : public MaterialParameter
    {
        glm::vec3 constantColor = glm::vec3(0.85f, 0.85, 0.7f);

        virtual void setShaderParameters(Shader* shader)
        {
            shader->setUniform("constantColor", constantColor);
        }
        
        virtual bool isLit() override
        {
            return false;
        }
    };

    // todo: is there a way to have this struct generate reflection data ...? probably have to use macro
    struct PBR : public MaterialParameter
    {
        virtual u32 getFlags()
        {
            u32 flags = 0u;
            if (albedo)
            {
                flags |= (u32)Flags::kHasDiffuseMap;
            }
            if (normal)
            {
                flags |= (u32)Flags::kHasNormalMap;
            }
            if (roughness)
            {
                flags |= (u32)Flags::kHasRoughnessMap;
            }
            if (metallic)
            {
                flags |= (u32)Flags::kHasMetallicMap;
            }
            if (metallicRoughness)
            {
                flags |= (u32)Flags::kHasMetallicRoughnessMap;
            }
            if (occlusion)
            {
                flags |= (u32)Flags::kHasOcclusionMap;
            }
            return flags;
        }

        virtual void renderUI() override;

        virtual void setShaderParameters(Shader* shader) override
        {
            shader->setUniform("materialInput.M_flags", getFlags());
            shader->setTexture("materialInput.M_albedo", albedo);
            shader->setTexture("materialInput.M_normal", normal);
            shader->setTexture("materialInput.M_roughness", roughness);
            shader->setTexture("materialInput.M_metallic", metallic);
            shader->setTexture("materialInput.M_metallicRoughness", metallicRoughness);
            shader->setTexture("materialInput.M_occlusion", occlusion);
            shader->setUniform("materialInput.M_kRoughness", kRoughness);
            shader->setUniform("materialInput.M_kMetallic", kMetallic);
            shader->setUniform("materialInput.M_kAlbedo", kAlbedo);
        }

        virtual bool isLit() override
        {
            return true;
        }

        Texture2DRenderable* albedo = nullptr;
        Texture2DRenderable* normal = nullptr;
        Texture2DRenderable* roughness = nullptr;
        Texture2DRenderable* metallic = nullptr;
        Texture2DRenderable* metallicRoughness = nullptr;
        Texture2DRenderable* occlusion = nullptr;

        glm::vec3 kAlbedo = glm::vec3(0.8);
        glm::vec3 kEmissive = glm::vec3(1.f);
        f32 kRoughness = 0.5f;
        f32 kMetallic = 0.0f;
    };

    template<typename BaseMaterialParameter>
    struct Emissive : BaseMaterialParameter
    {
        virtual void setShaderParameters(Shader* shader) override
        {
            BaseMaterialParameter::setShaderParameters(shader);
            shader->setTexture("M_emissive", emissive);
            shader->setUniform("M_kEmissive", kEmissive);
        }

        Texture2DRenderable* emissive = nullptr;
        float kEmissive = 1.f;
    };

    struct Lightmap
    {
        enum class Quality
        {
            kLow = 0,
            kHigh
        } quality;

        Texture2DRenderable* atlas = nullptr;
    };

    template <typename BaseMaterialParameter>
    struct Lightmapped : public BaseMaterialParameter
    {
        virtual void setShaderParameters(Shader* shader)
        {

        }

        Lightmap lightmap;
    };

    struct IMaterial : public Asset
    {
        using MaterialShaderMap = std::unordered_map<std::string, const char*>;

        enum class Type
        {
            kPBR = 0,
            kConstantColor,
            kLightmappedPBR,
            kEmissivePBR,
            kEmissiveConstantColor
        };

        IMaterial(const char* instanceName)
            : name(instanceName)
        { 
        }

        virtual void renderUI() { };
        virtual Shader* getMaterialShader() = 0;
        static std::string getAssetClassTypeDesc() { return std::string("IMaterial"); }
        virtual std::string getAssetObjectTypeDesc() override { return std::string("IMaterial"); }
        virtual void setShaderMaterialParameters() = 0;
        virtual bool isLit() = 0;

        // material shader map
        static MaterialShaderMap materialShaderMap;

        // instance name
        std::string name;
    };

    template <typename MaterialParameter>
    struct Material : public IMaterial
    {
        Material(const char* instanceName)
            : IMaterial(instanceName)
        {
            if (!shader)
            {
                auto entry = materialShaderMap.find(typeDesc);
                if (entry != materialShaderMap.end())
                {
                    shader = ShaderManager::getShader(entry->second);
                }
            }
        }

        virtual void renderUI() override { parameter.renderUI(); };
        virtual std::string getAssetObjectTypeDesc() override { return typeDesc; }
        static std::string getAssetClassTypeDesc() { return typeDesc; }
        virtual Shader* getMaterialShader() override { return shader; }
        virtual bool isLit() override { return parameter.isLit(); }

        virtual void setShaderMaterialParameters() override
        {
            parameter.setShaderParameters(shader);
        }

        static Shader* shader;
        static std::string typeDesc;
        MaterialParameter parameter;
    };

    typedef Material<PBR> PBRMaterial;
    typedef Material<Lightmapped<PBR>> LightmappedPBRMatl;
    typedef Material<Emissive<PBR>> EmissivePBRMatl;
    typedef Material<ConstantColor> ConstantColorMatl;
#else
    struct GpuMaterial {
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

    struct Material {
        enum class Flags : u32 {
            kHasAlbedoMap            = 1 << 0,
            kHasNormalMap            = 1 << 1,
            kHasMetallicRoughnessMap = 1 << 2,
            kHasOcclusionMap         = 1 << 3,
        };

        void renderUI();
        GpuMaterial buildGpuMaterial();

        std::string name;
        Texture2DRenderable* albedoMap = nullptr;
        Texture2DRenderable* normalMap = nullptr;
        Texture2DRenderable* metallicRoughnessMap = nullptr;
        Texture2DRenderable* occlusionMap = nullptr;
        glm::vec4 albedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
    };
#endif
};