#pragma once

#include <vector>
#include <map>

#include "Uniform.h"
#include "Shader.h"
#include "Texture.h"

#define CYAN_MAX_NUM_SAMPLERS 32
#define CYAN_MAX_NUM_BUFFERS 16

namespace Cyan
{
    struct MaterialInstance;

    struct TextureBinding
    {
        Uniform* m_sampler;
        Texture* m_tex;
    };

    struct UsedBindingPoints
    {
        u32 m_usedTexBindings;
        u32 m_usedBufferBindings;
    };

    struct GltfMaterial
    {
        Cyan::Texture* m_albedo;
        Cyan::Texture* m_normal;
        Cyan::Texture* m_metallicRoughness;
        Cyan::Texture* m_occlusion;
    };

    struct Material
    {
        // determine if this material type contains what kind of data
        enum DataFields
        {
            Lit = 0,
            Probe
        };

        using DataOffset = u32;

        void bindUniform(Uniform* _uniform);
        void bindSampler(Uniform* _sampler);
        void finalize();
        MaterialInstance* createInstance();

        Shader* m_shader;
        std::string m_name;
        std::map<UniformHandle, DataOffset> m_dataOffsetMap;
        std::vector<Uniform*> m_uniforms;
        Uniform* m_samplers[CYAN_MAX_NUM_SAMPLERS];
        // store the block index of each active ubo/ssbo 
        std::string      m_bufferBlocks[CYAN_MAX_NUM_BUFFERS];
        u32 m_numSamplers;
        u32 m_numBuffers;
        u32 m_bufferSize;
        // determine if this material should care about lighting
        u32 m_dataFieldsFlag;
        bool m_lit;
    };

    struct MaterialInstance
    {
        void bindTexture(const char* sampler, Texture* texture);
        void bindBuffer(const char* blockName, RegularBuffer* buffer);
        Texture* getTexture(const char* sampler);

        void set(const char* attribute, void* data);
        void set(const char* attribute, u32 data);
        void set(const char* attribute, i32 data);
        void set(const char* attribute, float data);
        u32 getAttributeOffset(UniformHandle handle);
        Shader* getShader()
        {
            return m_template->m_shader;
        }
        UsedBindingPoints bind();

        // material class
        Material* m_template;
        // { sampler, texture} bindings
        TextureBinding m_bindings[CYAN_MAX_NUM_SAMPLERS];
        // ubo/ssbo bindings
        RegularBuffer* m_bufferBindings[CYAN_MAX_NUM_BUFFERS];
        // material instance data buffer
        // Warning: every attributes has to be set
        UniformBuffer* m_uniformBuffer;
    };

    class SolidColorMaterial
    {
    public:
        SolidColorMaterial(glm::vec3& color)
            : m_color(color)
        {

        }
        glm::vec3 m_color;
        MaterialInstance* m_matl;
    };
}

/*
struct ShaderInputData
{

};

struct MaterialData
{
    std::vector<Texture*>
    std::vector<UniformData*> 
};

// TODO: Think about this way of doing material
class BaseMaterial {
public:
    virtual void create();
    virtual void bind();
    virtual void update();

    MaterialData m_materialData;
};

class PbrMaterial : public BaseMaterial {

    void setRoughnessConstant() { }
    void setMetallicConstant() { }
    void setAlbedo(Cyan::Texture* albedo) { m_matlInstance->bindTexture("diffuseMap[0]", albedo); }
    void setMetallicRoughness() { }
    virtual void bind() override { }

    Cyan::MaterialInstance* m_matlInstance;
};

class FlatColorMaterial : public BaseMaterial
{
    glm::vec4 color; 
};
*/