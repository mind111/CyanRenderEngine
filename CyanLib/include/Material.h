#pragma once

#include <vector>
#include <unordered_map>

#include "Shader.h"
#include "Texture.h"
#include "AssetFactory.h"

namespace Cyan
{
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
            kUsePrototypeTexture     = 1 << 6,
            kUseLightMap             = 1 << 7             
        };

        virtual void setShaderParameters(Shader* shader) = 0;
    };

    struct ConstantColor : public MaterialParameter
    {
        glm::vec3 constantColor = glm::vec3(0.85f, 0.85, 0.7f);

        virtual void setShaderParameters(Shader* shader)
        {
            shader->setUniform("constantColor", constantColor);
        }
    };

    // todo: is there a way to have this struct generate reflection data ...? probably have to use macro
    // for example
    // attribute 0 : name, type, data
    // attribute 1 : name, type, data
    // attribute 2 : name, type, data
    // attribute 3 : name, type, data
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

        // todo: is there a way to nicely abstract binding each member field, if we can do reflection, then we 
        // can loop through each member field and call shader->setUniform() on each field
        virtual void setShaderParameters(Shader* shader) override
        {
            shader->setUniform("M_flags", flags);
            shader->setTexture("M_albedo", albedo);
            shader->setTexture("M_normal", normal);
            shader->setTexture("M_roughness", roughness);
            shader->setTexture("M_metallic", metallic);
            shader->setTexture("M_metallicRoughness", metallicRoughness);
            shader->setTexture("M_occlusion", occlusion);
            shader->setUniform("M_kRoughness", kRoughness);
            shader->setUniform("M_kMetallic", kMetallic);
            shader->setUniform("M_kAlbedo", kAlbedo);
        }

        u32 flags = 0x0;
        Texture* albedo = nullptr;
        Texture* normal = nullptr;
        Texture* roughness = nullptr;
        Texture* metallic = nullptr;
        Texture* metallicRoughness = nullptr;
        Texture* occlusion = nullptr;
        f32 kRoughness = 0.6f;
        f32 kMetallic = 0.2f;
        glm::vec3 kAlbedo = glm::vec3(0.85f, 0.85, 0.7f);
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

        Texture* emissive = nullptr;
        float kEmissive = 1.f;
    };

    struct Lightmap
    {
        enum class Quality
        {
            kLow = 0,
            kHigh
        } quality;

        Texture* atlas = nullptr;
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
        using MaterialShaderMap = std::unordered_map<const char*, const char*>;

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

        virtual Shader* getMaterialShader() = 0;
        static std::string getAssetClassTypeDesc() { return std::string("IMaterial"); }
        virtual std::string getAssetObjectTypeDesc() override { return std::string("IMaterial"); }
        virtual void setShaderParameters() = 0;

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
                auto entry = materialShaderMap.find(typeDesc.c_str());
                if (entry != materialShaderMap.end())
                {
                    const char* shaderName = entry->second;
                    shader = ShaderManager::getShader(shaderName);
                }
            }
        }

        virtual std::string getAssetObjectTypeDesc() override { return typeDesc; }
        static std::string getAssetClassTypeDesc() { return typeDesc; }
        virtual Shader* getMaterialShader() override { return shader; }

        virtual void setShaderParameters() override
        {
            parameter.setShaderParameters(shader);
        }

        static Shader* shader;
        static std::string typeDesc;
        MaterialParameter parameter;
    };

    typedef Material<PBR> PBRMatl;
    typedef Material<Lightmapped<PBR>> LightmappedPBRMatl;
    typedef Material<Emissive<PBR>> EmissivePBRMatl;
    typedef Material<ConstantColor> ConstantColorMatl;
};