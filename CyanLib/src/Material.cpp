#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan 
{
    RenderableScene::Material MaterialBindless::buildGpuMaterial()
    {
        RenderableScene::Material outGpuMaterial = { };
        if (albedoMap != nullptr && albedoMap->isInited()) 
        {
            outGpuMaterial.albedoMap = albedoMap->gfxTexture->glHandle;
            outGpuMaterial.flag |= (u32)Flags::kHasAlbedoMap;
            if (glIsTextureHandleResidentARB(outGpuMaterial.albedoMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(outGpuMaterial.albedoMap);
            }
        }
        if (normalMap != nullptr && normalMap->isInited())
        {
            outGpuMaterial.normalMap = normalMap->gfxTexture->glHandle;
            outGpuMaterial.flag |= (u32)Flags::kHasNormalMap;
            if (glIsTextureHandleResidentARB(outGpuMaterial.normalMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(outGpuMaterial.normalMap);
            }
        }
        if (metallicRoughnessMap != nullptr && metallicRoughnessMap->isInited()) 
        {
            outGpuMaterial.metallicRoughnessMap = metallicRoughnessMap->gfxTexture->glHandle;
            outGpuMaterial.flag |= (u32)Flags::kHasMetallicRoughnessMap;
            if (glIsTextureHandleResidentARB(outGpuMaterial.metallicRoughnessMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(outGpuMaterial.metallicRoughnessMap);
            }
        }
        if (occlusionMap != nullptr && occlusionMap->isInited()) 
        {
            outGpuMaterial.occlusionMap = occlusionMap->gfxTexture->glHandle;
            outGpuMaterial.flag |= (u32)Flags::kHasOcclusionMap;
            if (glIsTextureHandleResidentARB(outGpuMaterial.occlusionMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(outGpuMaterial.occlusionMap);
            }
        }
        outGpuMaterial.albedo = albedo;
        outGpuMaterial.metallic = metallic;
        outGpuMaterial.roughness = roughness;
        outGpuMaterial.emissive = emissive;
        return outGpuMaterial;
    }
}