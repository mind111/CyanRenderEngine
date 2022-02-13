#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"

namespace Cyan
{
    void Material::bindUniform(Uniform* _uniform)
    {
        UniformHandle handle = Cyan::getUniformHandle(_uniform->m_name);
        CYAN_ASSERT(m_dataOffsetMap.find(handle) == m_dataOffsetMap.end(),"[ERROR] Uniform %s was already bound to material", _uniform->m_name)
        m_dataOffsetMap.insert(std::pair<UniformHandle, DataOffset>(handle, m_bufferSize));
        m_uniforms.push_back(_uniform);
        m_bufferSize += sizeof(UniformHandle);
        m_bufferSize += _uniform->getSize();
    }

    void Material::finalize()
    {
        m_bufferSize += 4; // to leave space for UniformBuffer::End 
    }

    void Material::bindSampler(Uniform* sampler)
    {
        m_samplers[m_numSamplers++] = sampler;
    }

    /*
        * when creating a new instance, by default all the user data should be
        cleared to 0, texture bindings should be cleared as well.
    */
    MaterialInstance* Material::createInstance()
    {
        MaterialInstance* instance = (MaterialInstance*)CYAN_ALLOC(sizeof(MaterialInstance));
        instance->m_template = this;
        instance->m_uniformBuffer = Cyan::createUniformBuffer(this->m_bufferSize);
        instance->m_uniformBuffer->clear();
        // init texture bindings
        for (u32 s = 0; s < m_numSamplers; ++s)
        {
            instance->m_bindings[s].m_sampler = m_samplers[s];
            instance->m_bindings[s].m_tex = nullptr;
        }
        for (u32 b = 0; b < m_numBuffers; ++b)
        {
            instance->m_bufferBindings[b] = 0;
        }
        // init data buffer
        for (auto entry : this->m_dataOffsetMap)
        {
            u32 uniformHandle = entry.first;
            u32 offset = entry.second;
            instance->m_uniformBuffer->reset(entry.second);
            instance->m_uniformBuffer->write(entry.first);
        }
        instance->m_uniformBuffer->reset(m_bufferSize - sizeof(UniformHandle));
        instance->m_uniformBuffer->finish();
        return instance;
    }

    void MaterialInstance::bindTexture(const char* sampler, Texture* texture) 
    { 
        for (u32 s = 0; s < m_template->m_numSamplers; ++s)
        {
            Uniform* u_sampler = m_template->m_samplers[s]; 
            if (strcmp(sampler, u_sampler->m_name) == 0)
            {
                // update the binding
                m_bindings[s].m_tex = texture;
                break;
            }
        }
    }

    void MaterialInstance::bindBuffer(const char* blockName, RegularBuffer* buffer) 
    { 
        for (u32 i = 0; i < CYAN_MAX_NUM_BUFFERS; ++i)
        {
            if (strcmp(blockName, m_template->m_bufferBlocks[i].c_str()) == 0)
            {
                m_bufferBindings[i] = buffer;
                break;
            }
        }
    }

    u32 MaterialInstance::getAttributeOffset(const char* attribute)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        if (!uniform)
        {
            cyanError("Uniform %s doesn't exist", attribute);
        }
        u32 sizeInBytes = uniform->getSize();
        return getAttributeOffset(handle);
    }

    u32 MaterialInstance::getAttributeOffset(UniformHandle handle)
    {
        // CYAN_ASSERT(m_template->m_dataOffsetMap.find(handle) != m_template->m_dataOffsetMap.end(), 
        //     "Attempting to set an attribute that does not exist")
        if (m_template->m_dataOffsetMap.find(handle) == m_template->m_dataOffsetMap.end())
        {
            return (u32)-1;
        }
        return m_template->m_dataOffsetMap[handle] + sizeof(UniformHandle);
    }
        
    Texture* MaterialInstance::getTexture(const char* sampler)
    {
        for (u32 s = 0; s < m_template->m_numSamplers; ++s)
        {
            Uniform* u_sampler = m_template->m_samplers[s]; 
            if (strcmp(sampler, u_sampler->m_name) == 0)
            {
                // update the binding
                return m_bindings[s].m_tex;
            }
        }
        return nullptr;
    }

    void MaterialInstance::set(const char* attribute, float data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        if (!uniform) return;
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        if (offset == (u32)-1)
        {
            return;
        }
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, u32 data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        if (!uniform) return;
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        if (offset == (u32)-1)
        {
            return;
        }
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, i32 data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        if (!uniform) return;
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        if (offset == (u32)-1) return;
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, const void* data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        if (!uniform) return;
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        if (offset == (u32)-1) return;
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data, sizeInBytes);
    }

    glm::vec3 MaterialInstance::getVec3(const char* attribute)
    {
        u32 offset = getAttributeOffset(attribute);
        m_uniformBuffer->reset(offset);
        return m_uniformBuffer->readVec3();
    }

    glm::vec4 MaterialInstance::getVec4(const char* attribute)
    {
        u32 offset = getAttributeOffset(attribute);
        m_uniformBuffer->reset(offset);
        return m_uniformBuffer->readVec4();
    }

    f32 MaterialInstance::getF32(const char* attribute)
    {
        u32 offset = getAttributeOffset(attribute);
        m_uniformBuffer->reset(offset);
        return m_uniformBuffer->readF32();
    }

    UsedBindingPoints MaterialInstance::bind()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // bind textures
        u32 textureUnit = 10;
        for (u32 s = 0; s < m_template->m_numSamplers; ++s)
        {
            if (m_bindings[s].m_tex)
            {
                ctx->setSampler(m_bindings[s].m_sampler, textureUnit);
                ctx->setTexture(m_bindings[s].m_tex, textureUnit++);
            }
        }
        // update instance data
        m_uniformBuffer->reset();
        for (;;)
        {
            UniformHandle handle = *(UniformHandle*)m_uniformBuffer->read(4);
            BREAK_WHEN(handle == UniformBuffer::End)
            Uniform* uniform = Cyan::getUniform(handle);
            u32 size = uniform->getSize();
            void* data = m_uniformBuffer->read(size);
            // TODO: this is redundant, how to optimize this
            Cyan::setUniform(uniform, data);
            ctx->setUniform(uniform);
        }
        return { 0, 0 };
    }

    Material* StandardPbrMaterial::m_standardPbrMatl = nullptr;
    StandardPbrMaterial::StandardPbrMaterial()
        : m_flags(0u)
    {
        // todo: make the initialization of pbr material deterministic so that we don't have to do this check everytime when creating a new matl instance
        if (!m_standardPbrMatl)
        {
            Shader* standardPBSShader = getShader("PBSShader");
            CYAN_ASSERT(standardPBSShader, "Standard PBS shader was not created!");
            m_standardPbrMatl = createMaterial(standardPBSShader);
        }
        m_materialInstance = m_standardPbrMatl->createInstance();
        // create a material instance
        glm::vec4 defaultBaseColor(0.9f, 0.9f, 0.9f, 1.f);
        m_materialInstance->set("uMatlData.flags", 0u);
        m_materialInstance->set("uMatlData.flatBaseColor", &defaultBaseColor.x);
        m_materialInstance->set("uMatlData.uniformSpecular", .5f);
        m_materialInstance->set("uMatlData.uniformRoughness", .5f);
        m_materialInstance->set("uMatlData.uniformMetallic", .5f);
        m_materialInstance->set("uMatlData.directDiffuseScale", 1.f);
        m_materialInstance->set("uMatlData.directSpecularScale", 1.f);
        m_materialInstance->set("uMatlData.indirectDiffuseScale", 1.f);
        m_materialInstance->set("uMatlData.indirectSpecularScale", 1.f);
        m_materialInstance->set("uMatlData.useSkybox", 0.f);
        m_materialInstance->set("gLighting.indirectDiffuseScale", 1.f);
        m_materialInstance->set("gLighting.indirectSpecularScale", 1.f);
        m_materialInstance->set("disneyReparam", 1.0f);
    }

    StandardPbrMaterial::StandardPbrMaterial(const PbrMaterialParam& param)
        : m_flags(0u)
    {
        if (!m_standardPbrMatl)
        {
            Shader* standardPBSShader = getShader("PBSShader");
            CYAN_ASSERT(standardPBSShader, "Standard PBR shader was not created!");
            m_standardPbrMatl = createMaterial(standardPBSShader);
        }
        m_materialInstance = m_standardPbrMatl->createInstance();
        // albedo
        if (param.baseColor)
        {
            m_flags |= Flags::kHasDiffuseMap;
            m_materialInstance->bindTexture("diffuseMaps[0]", param.baseColor);
        }
        else
        {
            m_materialInstance->set("uMatlData.flatColor", &param.flatBaseColor.x);
            if (param.usePrototypeTexture > .5f)
            {
                m_flags |= Flags::kUsePrototypeTexture;
            }
        }
        // normal
        if (param.normal)
        {
            m_flags |= Flags::kHasNormalMap;
            m_materialInstance->bindTexture("normalMap", param.normal);
        }
        // occlusion
        if (param.occlusion)
        {
            m_flags |= Flags::kHasOcclusionMap;
            m_materialInstance->bindTexture("aoMap", param.occlusion);
        }
        // roughness
        if (param.roughness)
        {
            m_flags |= Flags::kHasRoughnessMap;
            m_materialInstance->bindTexture("roughnessMap", param.roughness);
        }
        // metallic
        if (param.metallic)
        {
            m_flags |= Flags::kHasMetallicMap;
            m_materialInstance->bindTexture("metalnessMap", param.metallic);
        }
        // metallicRoughness map
        if (param.metallicRoughness)
        {
            m_flags |= Flags::kHasMetallicRoughnessMap;
            m_materialInstance->bindTexture("metallicRoughnessMap", param.metallicRoughness);
        }
        // specular
        if (param.kSpecular > 0.f)
            m_materialInstance->set("uMatlData.uniformSpecular", param.kSpecular);
        else
            m_materialInstance->set("uMatlData.uniformSpecular", .5f);

        m_materialInstance->set("uMatlData.uniformRoughness", param.kRoughness);
        m_materialInstance->set("uMatlData.uniformMetallic", param.kMetallic);
        m_materialInstance->set("uMatlData.directDiffuseScale", 1.f);
        m_materialInstance->set("uMatlData.directSpecularScale", 1.f);
        m_materialInstance->set("uMatlData.indirectDiffuseScale", param.indirectDiffuseScale);
        m_materialInstance->set("uMatlData.indirectSpecularScale", param.indirectSpecularScale);

        // baked lighting
        if (param.hasBakedLighting > .5f)
        {
            m_flags |= Flags::kUseLightMap;
            m_materialInstance->bindTexture("lightMap", param.lightMap);
        }
        m_materialInstance->set("disneyReparam", 1.0f);
        m_materialInstance->set("uMatlData.flags", m_flags);
    }
}