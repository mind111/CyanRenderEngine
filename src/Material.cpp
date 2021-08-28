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
        if (offset == (u32)-1)
        {
            return;
        }
        m_uniformBuffer->reset(offset);
        m_uniformBuffer->write(data);
    }

    void MaterialInstance::set(const char* attribute, void* data)
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
        m_uniformBuffer->write(data, sizeInBytes);
    }

    UsedBindingPoints MaterialInstance::bind()
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
        // bind buffers
        u32 bufferBinding = 0u;
        for (u32 s = 0; s < m_template->m_numBuffers; ++s)
        {
            if (m_bufferBindings[s])
            {
                glShaderStorageBlockBinding(m_template->m_shader->m_programId, s, bufferBinding);
                ctx->setBuffer(m_bufferBindings[s], bufferBinding++);
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
        return { textureUnit, bufferBinding };
    }
}