#pragma once

#include <vector>

#include "Texture.h"

enum class MaterialType
{
    BlinnPhong = 0,
    PBR = 1
};

class Material
{
public:
    Material() {}
    virtual ~Material() {}
    virtual MaterialType getMaterialType() = 0;
    virtual std::string getMaterialName() = 0;
};

class BlinnPhongMaterial : public Material
{
public:
    BlinnPhongMaterial();
    ~BlinnPhongMaterial();
    MaterialType getMaterialType() override;
    std::string getMaterialName() override { return mName; };

    void pushDiffuseMap(Texture* diffuseMap);
    void pushSpecularMap(Texture* specularMap);
    void setNormalMap(Texture* nm) { mNormalMap = nm; };

    Texture* getNormalMap();
    std::vector<Texture*>::iterator diffuseBegin();
    std::vector<Texture*>::iterator diffuseEnd();
    std::vector<Texture*>::iterator specularBegin();
    std::vector<Texture*>::iterator specularEnd();

    std::string mName;
private:
    std::vector<Texture*> mDiffuseMaps;
    std::vector<Texture*> mSpecularMaps;
    Texture* mNormalMap;
};