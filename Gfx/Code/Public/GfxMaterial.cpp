#include "GfxMaterial.h"

namespace Cyan 
{
    GfxMaterial::GfxMaterial(const char* materialAssetName, const char* pixelShaderFilePath)
    {
        bool bFoundVS = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFoundVS, "StaticMeshVS", ENGINE_SHADER_PATH "static_mesh_v.glsl");

        bool bFoundPS = false;
        std::string pixelShaderName(materialAssetName);
        pixelShaderName += std::string("PS");

        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFoundPS, pixelShaderName.c_str(), pixelShaderFilePath);
        m_pipeline = ShaderManager::createGfxPipeline(materialAssetName, vs, ps);

        // todo: build material parameters 
        GHPixelShader* GHPS = ps->getGHO();
        i32 parameterCount = 0;
        for (const auto& entry : GHPS->m_uniformMap)
        {
            const ShaderUniformDesc& desc = entry.second;
            // make sure that the uniform is marked as material parameter by verifying
            // the prefix "mp_"
            if (desc.name.find("mp_") != std::string::npos)
            {
                MaterialParameter* p = nullptr;
                m_parameterMap.insert({ desc.name, MaterialParameterDesc{ parameterCount++, desc } });
            }
        }
    }

    GfxMaterial::~GfxMaterial()
    {

    }

    GfxMaterialInstance::GfxMaterialInstance(const char* name, GfxMaterial* parent)
        : m_parent(parent)
    {
        i32 parameterCount = static_cast<i32>(m_parent->m_parameterMap.size());
        assert(parameterCount >= 0);
        m_parameters.resize(parameterCount);
        for (const auto& entry : m_parent->m_parameterMap)
        {
            MaterialParameter* p = nullptr;
            const auto& materialParameterDesc = entry.second;
            const auto& uniformDesc = materialParameterDesc.desc;
            switch (uniformDesc.type)
            {
            case ShaderUniformDesc::Type::kInt: p = new IntMaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kUint: p = new UintMaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kFloat: p = new FloatMaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kVec2: p = new Vec2MaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kVec3: p = new Vec3MaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kVec4: p = new Vec4MaterialParameter(uniformDesc.name.c_str()); break;
            case ShaderUniformDesc::Type::kSampler2D: p = new Texture2DMaterialParameter(uniformDesc.name.c_str()); break;
            default:
                assert(0);
            }
            m_parameters[materialParameterDesc.index] = p;
        }
    }

    GfxPipeline* GfxMaterialInstance::bind()
    {
        GfxPipeline* gfxp = m_parent->m_pipeline.get();
        gfxp->bind();
        for (auto p : m_parameters)
        {
            p->bind(gfxp->getPixelShader());
        }
        return gfxp;
    }

    void GfxMaterialInstance::unbind()
    {
        // todo: implement this
        GfxPipeline* gfxp = m_parent->m_pipeline.get();
        gfxp->unbind();
    }

#define SET_MATERIAL_INSTANCE_PARAMETER(parameterName, func, data)          \
    i32 parameterIndex = m_parent->m_parameterMap[parameterName].index;     \
    if (parameterIndex >= 0 && parameterIndex < m_parameters.size())        \
    {                                                                       \
        MaterialParameter* p = m_parameters[parameterIndex];                \
        if (p->bInstanced)                                                  \
        {                                                                   \
            p->func(data);                                                  \
        }                                                                   \
    }                                                                       \

    void GfxMaterialInstance::setInt(const char* parameterName, i32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setInt, data)
    }

    void GfxMaterialInstance::setUint(const char* parameterName, u32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setUint, data)
    }

    void GfxMaterialInstance::setFloat(const char* parameterName, f32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setFloat, data)
    }

    void GfxMaterialInstance::setVec2(const char* parameterName, const glm::vec2& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec2, data)
    }

    void GfxMaterialInstance::setVec3(const char* parameterName, const glm::vec3& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec3, data)
    }

    void GfxMaterialInstance::setVec4(const char* parameterName, const glm::vec4& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec4, data)
    }

    void GfxMaterialInstance::setTexture(const char* parameterName, GHTexture2D* texture)
    {
        i32 parameterIndex = m_parent->m_parameterMap[parameterName].index;
        if (parameterIndex >= 0 && parameterIndex < m_parameters.size())
        {
            MaterialParameter* p = m_parameters[parameterIndex];
            if (p->bInstanced)
            {                                                      
                p->setTexture(texture);                                    
            }                                                    
        }                                                              
    }
}
