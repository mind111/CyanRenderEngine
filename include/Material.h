#pragma once

#include <vector>

#include "Uniform.h"
#include "Shader.h"
#include "Texture.h"

namespace Cyan
{
    struct Material
    {
        struct TextureBinding
        {
            Uniform* m_sampler;
            Texture* m_tex;
        };

        void bindUniform(Uniform* _uniform);
        void addTexture(const char* samplerName, Texture* _tex);

        Shader* m_shader;
        std::vector<Uniform*> m_uniforms;
        std::vector<TextureBinding> m_bindings;
    };

    // TODO: impl this ...?
    struct MaterialInstance
    {
        struct TextureBinding
        {
            Uniform* m_sampler;
            Texture* m_tex;
        };

        void bindTexture() { }; 

        Material* parent;
        std::vector<TextureBinding> m_bindings;
    };
}