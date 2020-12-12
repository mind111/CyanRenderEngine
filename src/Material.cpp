#include "Material.h"

BlinnPhongMaterial::BlinnPhongMaterial()
{

}

BlinnPhongMaterial::~BlinnPhongMaterial()
{

}

MaterialType BlinnPhongMaterial::getMaterialType()
{
    return MaterialType::BlinnPhong;
}

Texture* BlinnPhongMaterial::getNormalMap()
{
    return mNormalMap;
}

std::vector<Texture*>::iterator BlinnPhongMaterial::diffuseBegin()
{
    return mDiffuseMaps.begin(); 
}

std::vector<Texture*>::iterator BlinnPhongMaterial::diffuseEnd()
{
    return mDiffuseMaps.end(); 
}

std::vector<Texture*>::iterator BlinnPhongMaterial::specularBegin()
{
    return mSpecularMaps.begin(); 
}

std::vector<Texture*>::iterator BlinnPhongMaterial::specularEnd()
{
    return mSpecularMaps.end(); 
}

void BlinnPhongMaterial::pushDiffuseMap(Texture* t)
{
    mDiffuseMaps.push_back(t);
}

void BlinnPhongMaterial::pushSpecularMap(Texture* t)
{
    mSpecularMaps.push_back(t);
}