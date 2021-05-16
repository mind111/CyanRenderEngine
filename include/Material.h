#pragma once

#include <vector>
#include <map>

#include "Uniform.h"
#include "Shader.h"
#include "Texture.h"

#define CYAN_MAX_SAMPLER 16

namespace Cyan
{
    struct TextureBinding
    {
        Uniform* m_sampler;
        Texture* m_tex;
    };

    struct Material
    {
        using DataOffset = u32;

        void bindUniform(Uniform* _uniform);
        void addTexture(const char* samplerName, Texture* _tex);

        Shader* m_shader;
        std::map<UniformHandle, DataOffset> m_instanceDataMap;
        std::vector<Uniform*> m_uniforms;
        Uniform* m_samplers[CYAN_MAX_SAMPLER];
        std::vector<TextureBinding> m_bindings;
    };

    // TODO: impl this ...?
    struct MaterialInstance
    {
        void bindTexture(const char* sampler, Texture* texture) { }
        void set(const char* attribute, void* data) { }

        Material* m_template;
        UniformBuffer* m_instanceData;
        std::vector<TextureBinding> m_bindings;
    };
}