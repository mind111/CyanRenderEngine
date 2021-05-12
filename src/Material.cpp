#include "Material.h"

namespace Cyan
{
    void Material::bindUniform(Uniform* _uniform)
    {
        m_uniforms.push_back(_uniform);
    }

    void Material::addTexture(const char* _samplerName, Texture* _tex)
    {
        auto findSampler = [&]() {
            Uniform* result = nullptr;
            for (Uniform* uniform : m_uniforms)
            {
                if (strcmp(uniform->m_name.c_str(),  _samplerName) == 0)
                {
                    return uniform;
                }
            }
            ASSERT(0) // Should not reach
            return result;
        };
        Uniform* sampler = findSampler(); 
        for (auto& binding : m_bindings)        
        {
            if (binding.m_sampler == sampler)
            {
                // printf("[Info]: Update texture binding for sampler %s", binding.m_sampler->m_name.c_str());
                binding.m_tex = _tex;
                return;
            }
        }
        m_bindings.push_back({ findSampler(), _tex });
    }
}