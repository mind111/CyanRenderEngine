#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <array>

#include "Asset.h"
#include "Texture.h"
#include "Shader.h"

#define MATERIAL_SOURCE_PATH "C:/dev/cyanRenderEngine/materials/"
#define MATERIAL_DEV 1

namespace Cyan 
{
    class GfxContext;
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

    struct MaterialParameter
    {
        MaterialParameter() { }
        MaterialParameter(const char* inName)
            : name(inName)
        {
        }
        virtual ~MaterialParameter() { }

        std::string name;
        bool bInstanced = true;

        virtual void bind(PixelShader* ps) = 0;

        virtual void setInt(i32 data) { assert(0); }
        virtual void setUint(u32 data) { assert(0); }
        virtual void setFloat(f32 data) { assert(0); }
        virtual void setVec2(const glm::vec2& data) { assert(0); }
        virtual void setVec3(const glm::vec3& data) { assert(0); }
        virtual void setVec4(const glm::vec4& data) { assert(0); }
        virtual void setTexture(Texture2D* texture) { assert(0); }
    };

    struct IntMaterialParameter : public MaterialParameter 
    {
        IntMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(-1)
        {
        }

        i32 data;

        virtual void setInt(i32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct UintMaterialParameter : public MaterialParameter 
    {
        UintMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(0)
        {
        }

        u32 data;

        virtual void setUint(u32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct FloatMaterialParameter : public MaterialParameter 
    {
        FloatMaterialParameter(const char* inName)
            : MaterialParameter(inName), data(0.f)
        {
        }

        f32 data;

        virtual void setFloat(f32 inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec2MaterialParameter : public MaterialParameter 
    {
        Vec2MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec2(0.f))
        {
        }

        glm::vec2 data;

        virtual void setVec2(const glm::vec2& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec3MaterialParameter : public MaterialParameter 
    {
        Vec3MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec3(0.f))
        {
        }

        glm::vec3 data;

        virtual void setVec3(const glm::vec3& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Vec4MaterialParameter : public MaterialParameter 
    {
        Vec4MaterialParameter(const char* inName)
            : MaterialParameter(inName), data(glm::vec4(0.f))
        {
        }

        glm::vec4 data;

        virtual void setVec4(const glm::vec4& inData) override { data = inData; }
        virtual void bind(PixelShader* ps) override { ps->setUniform(name.c_str(), data); }
    };

    struct Texture2DMaterialParameter : public MaterialParameter 
    {
        Texture2DMaterialParameter(const char* inName)
            : MaterialParameter(inName), texture(nullptr)
        {
        }

        Texture2D* texture;

        virtual void setTexture(Texture2D* inTexture) override { texture = inTexture; }
        virtual void bind(PixelShader* ps) override { ps->setTexture(name.c_str(), texture->getGfxResource()); }
    };

    class MaterialInstance;

    /**
     * This implementation of material works but has insane amount of overhead due to 
        excessive use of polymorphism, need to revisit and optimize later. Perfect example of
        over engineering but I like it :)
     */
    class NewMaterial : public Asset
    {
    public:
        friend class MaterialInstance;

        using SetupDefaultInstance = std::function<void(MaterialInstance*)>;
        NewMaterial(const char* name, const char* materialSourcePath, const SetupDefaultInstance& setupDefaultInstance);
        virtual ~NewMaterial();

        static const char* getAssetTypeName() { return "Material"; }

        void bind(GfxContext* ctx);

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

    protected:
        std::shared_ptr<PixelPipeline> m_pipeline = nullptr;
        struct MaterialParameterDesc
        {
            i32 index = -1;
            Shader::UniformDesc desc = { };
        };
        std::unordered_map<std::string, MaterialParameterDesc> m_parameterMap;
        std::function<void(NewMaterial*)> m_setupDefaultInstance;
        std::unique_ptr<MaterialInstance> m_defaultInstance = nullptr;

        static constexpr const char* m_defaultOpaqueMaterial = R"(
            #version 450 core

            layout(location = 0) out vec3 outAlbedo;
            layout(location = 1) out vec3 outNormal;
            layout(location = 2) out vec3 outMetallicRoughness;

            in VSOutput 
            {
                vec3 viewSpacePosition;
                vec3 worldSpacePosition;
                vec3 worldSpaceNormal;
                vec3 worldSpaceTangent;
                flat float tangentSpaceHandedness;
                vec2 texCoord0;
                vec2 texCoord1;
                vec3 vertexColor;
            } psIn;

            struct Material
            {
                vec3 albedo;
                vec3 normal;
                float roughness;
                float metallic;
                float occlusion;
            };

            vec4 tangentSpaceToWorldSpace(vec3 tangent, vec3 bitangent, vec3 worldSpaceNormal, vec3 tangentSpaceNormal) {   
                mat4 tbn;
                tbn[0] = vec4(tangent, 0.f);
                tbn[1] = vec4(bitangent, 0.f);
                tbn[2] = vec4(worldSpaceNormal, 0.f);
                tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
                return tbn * vec4(tangentSpaceNormal, 0.f);
            }

            uniform sampler2D mp_albedoMap;
            uniform sampler2D mp_normalMap;
            uniform sampler2D mp_metallicRoughnessMap;
            uniform sampler2D mp_emissiveMap;
            uniform sampler2D mp_occlusionMap;
            uniform vec4 mp_albedo;
            uniform float mp_metallic;
            uniform float mp_roughness;
            uniform float mp_emissive;
            uniform uint mp_flag;

            const uint kHasAlbedoMap            = 1 << 0;
            const uint kHasNormalMap            = 1 << 1;
            const uint kHasMetallicRoughnessMap = 1 << 2;
            const uint kHasOcclusionMap         = 1 << 3;

            Material calcMaterialProperties(vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec2 texCoord) 
            {
                Material outMaterial;
                
                outMaterial.normal = worldSpaceNormal;
                if ((mp_flag & kHasNormalMap) != 0u) 
                {
                    vec3 tangentSpaceNormal = texture(mp_normalMap, texCoord).xyz;
                    // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
                    tangentSpaceNormal = normalize(tangentSpaceNormal * 2.f - 1.f);
                    // Covert normal from tangent frame to camera space
                    outMaterial.normal = normalize(tangentSpaceToWorldSpace(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, tangentSpaceNormal).xyz);
                }

                outMaterial.albedo = mp_albedo.rgb;
                if ((mp_flag & kHasAlbedoMap) != 0u) 
                {
                    outMaterial.albedo = texture(mp_albedoMap, texCoord).rgb;
                    // from sRGB to linear space if using a texture
                    outMaterial.albedo = vec3(pow(outMaterial.albedo.r, 2.2f), pow(outMaterial.albedo.g, 2.2f), pow(outMaterial.albedo.b, 2.2f));
                }

                // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
                float roughness = mp_roughness, metallic = mp_metallic;
                if ((mp_flag & kHasMetallicRoughnessMap) != 0u)
                {
                    vec2 metallicRoughness = texture(mp_metallicRoughnessMap, texCoord).gb;
                    roughness = metallicRoughness.x;
                    // roughness = roughness * roughness;
                    metallic = metallicRoughness.y; 
                }
                outMaterial.roughness = roughness;
                outMaterial.metallic = metallic;

                outMaterial.occlusion = 1.f;
                if ((mp_flag & kHasOcclusionMap) != 0u) 
                {
                    outMaterial.occlusion = texture(mp_occlusionMap, texCoord).r;
                    outMaterial.occlusion = pow(outMaterial.occlusion, 3.0f);
                }
                return outMaterial;
            }

            void main()
            {
                vec3 worldSpaceTangent = normalize(psIn.worldSpaceTangent);
                vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
                worldSpaceTangent = normalize(worldSpaceTangent - dot(worldSpaceNormal, worldSpaceTangent) * worldSpaceNormal); 
                vec3 worldSpaceBitangent = normalize(cross(worldSpaceNormal, worldSpaceTangent)) * psIn.tangentSpaceHandedness;

                Material material = calcMaterialProperties(worldSpaceNormal, worldSpaceTangent, worldSpaceBitangent, psIn.texCoord0);

                outAlbedo = material.albedo;
                outNormal = material.normal * .5f + .5f;
                outMetallicRoughness = vec3(material.metallic, material.roughness, 0.f);
            }
        )";
    };

    class MaterialInstance : public Asset
    {
    public:
        friend class NewMaterial;

        MaterialInstance(const char* name, NewMaterial* parent);
        ~MaterialInstance() { }

        void setInt(const char* parameterName, i32 data);
        void setUint(const char* parameterName, u32 data);
        void setFloat(const char* parameterName, f32 data);
        void setVec2(const char* parameterName, const glm::vec2& data);
        void setVec3(const char* parameterName, const glm::vec3& data);
        void setVec4(const char* parameterName, const glm::vec4& data);
        void setTexture(const char* parameterName, Texture2D* texture);

    private:
        NewMaterial* m_parent = nullptr;
        std::vector<MaterialParameter*> m_parameters;
    };
};