#include "Common.h"
#include "GfxContext.h"
#include "Material.h"
#include "Scene.h"

namespace Cyan 
{
    Material::Material(const char* name, const char* pixelShaderFilePath, const SetupDefaultInstance& setupDefaultInstance)
        : Asset(name)
    {
        CreateVS(vs, "StaticMeshVS", SHADER_SOURCE_PATH "static_mesh_v.glsl");
        std::string pixelShaderName(m_name);
        pixelShaderName += std::string("PS");
        CreatePS(ps, pixelShaderName.c_str(), pixelShaderFilePath);
        m_pipeline = std::make_shared<PixelPipeline>(m_name.c_str(), vs, ps);

        // todo: build material parameters 
        i32 parameterCount = 0;
        for (const auto& entry : ps->m_uniformMap)
        {
            const Shader::UniformDesc& desc = entry.second;
            // make sure that the uniform is marked as material parameter by verifying
            // the prefix "mp_"
            if (desc.name.find("mp_") != std::string::npos)
            {
                MaterialParameter* p = nullptr;
                m_parameterMap.insert({ desc.name, MaterialParameterDesc{ parameterCount++, desc } });
            }
        }

        // create and setup the default instance
        std::string instanceName = "Default_" + m_name;
        m_defaultInstance = std::make_unique<MaterialInstance>(instanceName.c_str(), this);
        setupDefaultInstance(m_defaultInstance.get());
    }

    Material::~Material()
    {

    }

    void Material::bind(GfxContext* ctx, const glm::mat4& localToWorldMatrix, const SceneCamera::ViewParameters& viewParameters)
    {
        m_pipeline->bind(ctx);
        m_pipeline->setUniform("localToWorld", localToWorldMatrix);
        viewParameters.setShaderParameters(m_pipeline.get());
        for (auto p : m_defaultInstance->m_parameters)
        {
            p->bind(m_pipeline->m_pixelShader);
        }
    }

#define SET_MATERIAL_PARAMETER(parameterName, func, data)                               \
    i32 parameterIndex = m_parameterMap[parameterName].index;                           \
    if (parameterIndex >= 0 && parameterIndex < m_defaultInstance->m_parameters.size()) \
    {                                                                                   \
        MaterialParameter* p = m_defaultInstance->m_parameters[parameterIndex];         \
        p->func(data);                                                                  \
    }                                                                                   \

    void Material::setInt(const char* parameterName, i32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setInt, data)
    }

    void Material::setUint(const char* parameterName, u32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setInt, data)
    }

    void Material::setFloat(const char* parameterName, f32 data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setFloat, data)
    }

    void Material::setVec2(const char* parameterName, const glm::vec2& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec2, data)
    }

    void Material::setVec3(const char* parameterName, const glm::vec3& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec3, data)
    }

    void Material::setVec4(const char* parameterName, const glm::vec4& data)
    {
        SET_MATERIAL_PARAMETER(parameterName, setVec4, data)
    }

    void Material::setTexture(const char* parameterName, Texture2D* texture)
    {
        i32 parameterIndex = m_parameterMap[parameterName].index;
        if (parameterIndex >= 0 && parameterIndex < m_defaultInstance->m_parameters.size())
        {
            MaterialParameter* p = m_defaultInstance->m_parameters[parameterIndex];
            p->setTexture(texture);                                    
        }                                                              
    }

    MaterialInstance::MaterialInstance(const char* name, Material* parent)
        : Asset(name), m_parent(parent)
    {
        i32 parameterCount = m_parent->m_parameterMap.size();
        assert(parameterCount >= 0);
        m_parameters.resize(parameterCount);
        for (const auto& entry : m_parent->m_parameterMap)
        {
            MaterialParameter* p = nullptr;
            const auto& materialParameterDesc = entry.second;
            const auto& uniformDesc = materialParameterDesc.desc;
            switch (uniformDesc.type)
            {
            case Shader::UniformDesc::Type::kInt: p = new IntMaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kUint: p = new UintMaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kFloat: p = new FloatMaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kVec2: p = new Vec2MaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kVec3: p = new Vec3MaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kVec4: p = new Vec4MaterialParameter(uniformDesc.name.c_str()); break;
            case Shader::UniformDesc::Type::kSampler2D: p = new Texture2DMaterialParameter(uniformDesc.name.c_str()); break;
            default:
                assert(0);
            }
            m_parameters[materialParameterDesc.index] = p;
        }
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

    void MaterialInstance::setInt(const char* parameterName, i32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setInt, data)
    }

    void MaterialInstance::setUint(const char* parameterName, u32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setUint, data)
    }

    void MaterialInstance::setFloat(const char* parameterName, f32 data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setFloat, data)
    }

    void MaterialInstance::setVec2(const char* parameterName, const glm::vec2& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec2, data)
    }

    void MaterialInstance::setVec3(const char* parameterName, const glm::vec3& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec3, data)
    }

    void MaterialInstance::setVec4(const char* parameterName, const glm::vec4& data)
    {
        SET_MATERIAL_INSTANCE_PARAMETER(parameterName, setVec4, data)
    }

    void MaterialInstance::setTexture(const char* parameterName, Texture2D* texture)
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