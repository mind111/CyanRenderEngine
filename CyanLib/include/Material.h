#pragma once

#include <vector>
#include <map>

#include "Uniform.h"
#include "Shader.h"
#include "Texture.h"
#include "AssetFactory.h"

#define CYAN_MAX_NUM_SAMPLERS 32
#define CYAN_MAX_NUM_BUFFERS 16

namespace Cyan
{
    struct MaterialInstance;

    struct TextureBinding
    {
        Uniform* m_sampler;
        Texture* m_tex;
    };

    struct UsedBindingPoints
    {
        u32 m_usedTexBindings;
        u32 m_usedBufferBindings;
    };

    struct GltfMaterial
    {
        Cyan::Texture* m_albedo;
        Cyan::Texture* m_normal;
        Cyan::Texture* m_metallicRoughness;
        Cyan::Texture* m_occlusion;
    };

    // interpret parameters in traditional obj material as pbr
    struct ObjMaterial
    {
        glm::vec3 diffuse;
        glm::vec3 specular; // how to use this ...?
        f32       kRoughness;
        f32       kMetalness;
    };
#if 0
    struct Material
    {
        // determine if this material type contains what kind of data
        enum DataFields
        {
            Lit = 0,
            Probe
        };

        using DataOffset = u32;

        void bindUniform(Uniform* _uniform);
        void bindSampler(Uniform* _sampler);
        void finalize();
        MaterialInstance* createInstance();

        Shader* m_shader;
        std::string m_name;
        std::map<UniformHandle, DataOffset> m_dataOffsetMap;
        std::vector<Uniform*> m_uniforms;
        Uniform* m_samplers[CYAN_MAX_NUM_SAMPLERS];
        // store the block index of each active ubo/ssbo 
        std::string      m_bufferBlocks[CYAN_MAX_NUM_BUFFERS];
        u32 m_numSamplers;
        u32 m_numBuffers;
        u32 m_bufferSize;
        // determine if this material should care about lighting
        u32 m_dataFieldsFlag;
        bool m_lit;
    };

    struct MaterialInstance
    {
        void bindTexture(const char* sampler, Texture* texture);
        void bindBuffer(const char* blockName, RegularBuffer* buffer);
        Texture* getTexture(const char* sampler);

        void set(const char* attribute, const void* data);
        void set(const char* attribute, u32 data);
        void set(const char* attribute, i32 data);
        void set(const char* attribute, float data);
        glm::vec3 getVec3(const char* attribute);
        glm::vec4 getVec4(const char* attribute);
        u32 getU32(const char* attribute);
        f32 getF32(const char* attribute);
        u32 getAttributeOffset(const char* attribute);
        u32 getAttributeOffset(UniformHandle handle);
        Shader* getMaterialShader()
        {
            return m_template->m_shader;
        }
        UsedBindingPoints bind();

        // material class
        Material* m_template;
        // { sampler, texture} bindings
        TextureBinding m_bindings[CYAN_MAX_NUM_SAMPLERS];
        // ubo/ssbo bindings
        RegularBuffer* m_bufferBindings[CYAN_MAX_NUM_BUFFERS];
        // material instance data buffer
        UniformBuffer* m_uniformBuffer;
    };

    struct PbrMaterialParam
    {
        glm::vec4      flatBaseColor;
        Cyan::Texture* baseColor;
        Cyan::Texture* normal;
        Cyan::Texture* metallicRoughness;
        Cyan::Texture* metallic;
        Cyan::Texture* roughness;
        Cyan::Texture* occlusion;
        Cyan::Texture* lightMap;
        f32            kRoughness;
        f32            kMetallic;
        f32            kSpecular;
        f32            indirectDiffuseScale;
        f32            indirectSpecularScale;
        // flags
        f32            hasBakedLighting;
        f32            usePrototypeTexture;
    };

    class StandardPbrMaterial
    {
    public:
        enum Flags
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

        StandardPbrMaterial();
        StandardPbrMaterial(const PbrMaterialParam& param);
        ~StandardPbrMaterial()
        {

        }

        u32 m_flags;
        static Material*    m_standardPbrMatl;
        MaterialInstance* m_materialInstance;
    };
#endif
}

namespace Cyan
{
    struct ShadingParameter
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

        virtual void bindForDraw(Shader* shader) = 0;
    };

    struct ConstantColor : public ShadingParameter
    {
        glm::vec3 constantColor = glm::vec3(0.85f, 0.85, 0.7f);

        virtual void bindForDraw(Shader* shader)
        {
            shader->setUniformVec3("constantColor", &constantColor.x);
        }
    };

    struct PBR : public ShadingParameter
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
        virtual void bindForDraw(Shader* shader) override
        {
            shader->setUniform1f("kRoughness", kRoughness);
            shader->setUniform1f("kMetallic", kMetallic);
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

    template<typename BaseShadingParameter>
    struct Emissive : BaseShadingParameter
    {
        virtual void bindForDraw(Shader* shader) override
        {

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

    template <typename BaseShadingParameter>
    struct Lightmapped : public BaseShadingParameter
    {
        virtual void bindForDraw(Shader* shader)
        {

        }

        Lightmap lightmap;
    };

    struct IMaterial : public Asset
    {
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
        { }

        virtual Shader* getMaterialShader() = 0;
        virtual std::string getAssetObjectTypeDesc() override { return std::string("BaseMaterial"); }
        virtual void bindForDraw() = 0;

        // instance name
        std::string name;
    };

    template <typename ShadingParameter>
    struct Material : public IMaterial
    {
        Material(const char* instanceName)
            : IMaterial(instanceName)
        {
            if (!shader)
            {
                shader = ShaderManager::getShader(typeDesc.c_str());
            }
        }

        virtual std::string getAssetObjectTypeDesc() override { return typeDesc; }
        static std::string getAssetClassTypeDesc() { return typeDesc; }
        virtual Shader* getMaterialShader() override { return shader; }

        virtual void bindForDraw() override
        {
            parameter.bindForDraw(shader);
        }

        static Shader* shader;
        static std::string typeDesc;
        ShadingParameter parameter;
    };

    typedef Material<PBR> PBRMatl;
    typedef Material<Lightmapped<PBR>> LightmappedPBRMatl;
    typedef Material<Emissive<PBR>> EmissivePBRMatl;
    typedef Material<ConstantColor> ConstantColorMatl;
};