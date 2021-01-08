#pragma once

#include <vector>

#include "Texture.h"

enum class MaterialType
{
    None = 0,
    BlinnPhong = 1,
    Pbr = 2
};

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