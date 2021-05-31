#pragma once

#include <vector>
#include <map>

#include "Uniform.h"
#include "Shader.h"
#include "Texture.h"

#define CYAN_MAX_NUM_SAMPLERS 16

namespace Cyan
{
    struct MaterialInstance;

    struct TextureBinding
    {
        Uniform* m_sampler;
        Texture* m_tex;
    };

    struct Material
    {
        using DataOffset = u32;

        void bindUniform(Uniform* _uniform);
        void bindSampler(Uniform* _sampler);
        void finalize();
        MaterialInstance* createInstance();

        Shader* m_shader;
        std::map<UniformHandle, DataOffset> m_dataOffsetMap;
        std::vector<Uniform*> m_uniforms;
        Uniform* m_samplers[CYAN_MAX_NUM_SAMPLERS];
        u32 m_numSamplers;
        u32 m_bufferSize;
    };

    struct MaterialInstance
    {
        void bindTexture(const char* sampler, Texture* texture);
        void set(const char* attribute, void* data);
        void set(const char* attribute, u32 data);
        void set(const char* attribute, float data);
        u32 getAttributeOffset(UniformHandle handle);
        u32 bind();

        // material class
        Material* m_template;
        // {sampler, texture} bindings
        TextureBinding m_bindings[CYAN_MAX_NUM_SAMPLERS];
        // material instance data buffer
        // Warning: every attributes has to be set
        UniformBuffer* m_uniformBuffer;
    };
}