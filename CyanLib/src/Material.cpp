#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan 
{
    void Material::setShaderParameters(PixelShader* ps) 
    {
        u32 flag = 0;
        if (albedoMap != nullptr && albedoMap->isInited())
        {
            ps->setTexture("materialDesc.albedoMap", albedoMap->getGfxResource());
            flag |= (u32)Flags::kHasAlbedoMap;
        }
        if (normalMap != nullptr && normalMap->isInited())
        {
            ps->setTexture("materialDesc.normalMap", normalMap->getGfxResource());
            flag |= (u32)Flags::kHasNormalMap;
        }
        if (metallicRoughnessMap != nullptr && metallicRoughnessMap->isInited())
        {
            ps->setTexture("materialDesc.metallicRoughnessMap", metallicRoughnessMap->getGfxResource());
            flag |= (u32)Flags::kHasMetallicRoughnessMap;
        }
        if (occlusionMap != nullptr && occlusionMap->isInited())
        {
            ps->setTexture("materialDesc.occlusionMap", occlusionMap->getGfxResource());
            flag |= (u32)Flags::kHasOcclusionMap;
        }
        ps->setUniform("materialDesc.albedo", albedo);
        ps->setUniform("materialDesc.metallic", metallic);
        ps->setUniform("materialDesc.roughness", roughness);
        ps->setUniform("materialDesc.emissive", emissive);
        ps->setUniform("materialDesc.flag", flag);
    }

    NewMaterial::NewMaterial(const char* name, const char* pixelShaderFilePath)
        : Asset(name)
    {
        CreateVS(vs, "StaticMeshVS", SHADER_SOURCE_PATH "static_mesh_v.glsl");
        std::string pixelShaderName(m_name);
        pixelShaderName += std::string("PS");
        CreatePS(ps, pixelShaderName.c_str(), pixelShaderFilePath);
        m_pipeline = std::make_shared<PixelPipeline>(m_name.c_str(), vs, ps);

        // todo: build material parameters 
        for (const auto& entry : ps->m_uniformMap)
        {
            const Shader::UniformDesc& desc = entry.second;
            // make sure that the uniform is marked as material parameter by verifying
            // the prefix "mp_"
            if (desc.name.find("mp_") != std::string::npos)
            {
                MaterialParameter* p = nullptr;
                switch (desc.type)
                {
                case Shader::UniformDesc::Type::kInt: p = new IntMaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kUint: p = new UintMaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kFloat: p = new FloatMaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kVec2: p = new Vec2MaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kVec3: p = new Vec3MaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kVec4: p = new Vec4MaterialParameter(desc.name.c_str()); break;
                case Shader::UniformDesc::Type::kSampler2D: p = new Texture2DMaterialParameter(desc.name.c_str()); break;
                default:
                    assert(0);
                }
                m_parameters.push_back(p);
                m_parameterMap.insert({ p->name, m_parameters.size() - 1 });
            }
        }
    }

    NewMaterial::~NewMaterial()
    {

    }

    void NewMaterial::bind(GfxContext* ctx)
    {
        m_pipeline->bind(ctx);
        for (auto p : m_parameters)
        {
            p->bind(m_pipeline->m_pixelShader);
        }
    }

#define SET_MATERIAL_PARAMETER(parameterName, func, data)            \
    i32 parameterIndex = m_parameterMap[parameterName];              \
    if (parameterIndex >= 0 && parameterIndex < m_parameters.size()) \
    {                                                                \
        MaterialParameter* p = m_parameters[parameterIndex];         \
        p->func(data);                                               \
    }                                                                \

    void NewMaterial::setInt(const char* parameterName, i32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setInt, data)
    }

    void NewMaterial::setUint(const char* parameterName, u32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setInt, data)
    }

    void NewMaterial::setFloat(const char* parameterName, f32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setFloat, data)
    }

    void NewMaterial::setVec2(const char* parameterName, const glm::vec2& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec2, data)
    }

    void NewMaterial::setVec3(const char* parameterName, const glm::vec3& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec3, data)
    }

    void NewMaterial::setVec4(const char* parameterName, const glm::vec4& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec4, data)
    }

    void NewMaterial::setTexture(Texture2D* texture)
    {
    }
}