#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan 
{
    void Material::setShaderParameters(PixelShader* ps) 
    {
        u32 flag = 0;
        if (albedoMap != nullptr && albedoMap->isInited())
        {
            ps->setTexture("materialDesc.albedoMap", albedoMap->getGfxResource());
            flag |= (u32)Flags::kHasAlbedoMap;
        }
        if (normalMap != nullptr && normalMap->isInited())
        {
            ps->setTexture("materialDesc.normalMap", normalMap->getGfxResource());
            flag |= (u32)Flags::kHasNormalMap;
        }
        if (metallicRoughnessMap != nullptr && metallicRoughnessMap->isInited())
        {
            ps->setTexture("materialDesc.metallicRoughnessMap", metallicRoughnessMap->getGfxResource());
            flag |= (u32)Flags::kHasMetallicRoughnessMap;
        }
        if (occlusionMap != nullptr && occlusionMap->isInited())
        {
            ps->setTexture("materialDesc.occlusionMap", occlusionMap->getGfxResource());
            flag |= (u32)Flags::kHasOcclusionMap;
        }
        ps->setUniform("materialDesc.albedo", albedo);
        ps->setUniform("materialDesc.metallic", metallic);
        ps->setUniform("materialDesc.roughness", roughness);
        ps->setUniform("materialDesc.emissive", emissive);
        ps->setUniform("materialDesc.flag", flag);
    }
}