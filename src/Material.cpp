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

    void Material::bindSampler(Uniform* _sampler)
    {
        m_samplers[m_numSamplers++] = _sampler;
    }

    MaterialInstance* Material::createInstance()
    {
        MaterialInstance* instance = (MaterialInstance*)CYAN_ALLOC(sizeof(MaterialInstance));
        instance->m_template = this;
        instance->m_uniformBuffer = Cyan::createUniformBuffer(this->m_bufferSize);
        // init texture bindings
        for (u32 s = 0; s < m_numSamplers; ++s)
        {
            instance->m_bindings[s].m_sampler = m_samplers[s];
            instance->m_bindings[s].m_tex = nullptr;
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

    u32 MaterialInstance::getAttributeOffset(UniformHandle handle)
    {
        return m_template->m_dataOffsetMap[handle] + sizeof(UniformHandle);
    }

    void MaterialInstance::set(const char* attribute, float data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, u32 data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, void* data)
    {
        UniformHandle handle = Cyan::getUniformHandle(attribute);
        Uniform* uniform = Cyan::getUniform(handle);
        u32 sizeInBytes = uniform->getSize();
        u32 offset = getAttributeOffset(handle);
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data, sizeInBytes);
    }

    u32 MaterialInstance::bind()
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // bind textures
        u32 textureUnit = 0;
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
        return textureUnit;
    }


    // void Material::bindTexture(const char* _samplerName, Texture* _tex)
    // {
    //     auto findSampler = [&]() {
    //         Uniform* result = nullptr;
    //         for (Uniform* uniform : m_uniforms)
    //         {
    //             if (strcmp(uniform->m_name, _samplerName) == 0)
    //             {
    //                 return uniform;
    //             }
    //         }
    //         CYAN_ASSERT(0, "should not reach") // Should not reach
    //         return result;
    //     };
    //     Uniform* sampler = findSampler(); 
    //     for (auto& binding : m_bindings)        
    //     {
    //         if (binding.m_sampler == sampler)
    //         {
    //             // printf("[Info]: Update texture binding for sampler %s", binding.m_sampler->m_name.c_str());
    //             binding.m_tex = _tex;
    //             return;
    //         }
    //     }
    //     m_bindings.push_back({ findSampler(), _tex });
    // }
}