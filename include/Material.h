#pragma once

#include <vector>

#include "Texture.h"

enum class MaterialType
{
    None = 0,
    BlinnPhong = 1,
    Pbr = 2
};

struct Uniform
{
    enum UniformType
    {
        u_float = 0,
        u_int,
        u_vec3,
        u_vec4,
        u_mat4,
        u_sampler2D,
        u_samplerCube,
        u_undefined // Error
    };

    UniformType m_type;
    char* m_name;
    void* m_valuePtr;

    u32 getSize(UniformType _type)
    {
        u32 size;
        switch(_type)
        {
            case u_float:
                size = 4;
                break;
            case u_int:
                size = 4;
                break;
            case u_vec3:
                size = 4 * 3;
                break;
            case u_vec4:
                size = 4 * 4;
                break;
            case u_mat4:
                size = 4 * 4 * 4;
                break;
            default:
                break;
        }
        return size;
    }
};

struct Material
{
    struct TextureBinding
    {
        char* m_sampler;
        Texture* m_tex;
    };

    void pushUniform(Uniform* _uniform)
    {
        m_uniforms.push_back(_uniform);
    }

    void pushTexture(char* samplerName, Texture* _tex)
    {
        m_bindings.push_back( {samplerName, _tex} );
    }

    Shader* m_shader;
    std::vector<Uniform*> m_uniforms;
    std::vector<TextureBinding> m_bindings;
};

/*
struct Material
{
    std::string m_name;
    virtual MaterialType m_type() = 0; 
};

struct BlinnPhongMaterial : Material
{
    virtual MaterialType m_type() override { return MaterialType::BlinnPhong; }

    std::vector<Texture*> m_diffuseMaps;
    std::vector<Texture*> m_specularMaps;
    std::vector<Texture*> m_emissionMaps;

    Texture* m_normalMap;
    Texture* m_aoMap;
    Texture* m_roughnessMap;
};

struct PbrMaterial : Material
{
    virtual MaterialType m_type() override { return MaterialType::Pbr; }

    std::vector<Texture*> m_diffuseMaps;
    std::vector<Texture*> m_specularMaps;
    std::vector<Texture*> m_emissionMaps;

    Texture* m_normalMap;
    Texture* m_aoMap;
    Texture* m_roughnessMap;
    Texture* m_metalnessMap;
};
*/