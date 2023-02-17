#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan 
{
#if 0
    void Material::renderUI() {

    }

    GpuMaterial Material::buildGpuMaterial() {
        GpuMaterial matl = { };
        if (albedoMap != nullptr) 
        {
            matl.albedoMap = albedoMap->glHandle;
            matl.flag |= (u32)Flags::kHasAlbedoMap;
            if (glIsTextureHandleResidentARB(matl.albedoMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(matl.albedoMap);
            }
        }
        if (normalMap != nullptr) 
        {
            matl.normalMap = normalMap->glHandle;
            matl.flag |= (u32)Flags::kHasNormalMap;
            if (glIsTextureHandleResidentARB(matl.normalMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(matl.normalMap);
            }
        }
        if (metallicRoughnessMap != nullptr) 
        {
            matl.metallicRoughnessMap = metallicRoughnessMap->glHandle;
            matl.flag |= (u32)Flags::kHasMetallicRoughnessMap;
            if (glIsTextureHandleResidentARB(matl.metallicRoughnessMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(matl.metallicRoughnessMap);
            }
        }
        if (occlusionMap != nullptr) 
        {
            matl.occlusionMap = occlusionMap->glHandle;
            matl.flag |= (u32)Flags::kHasOcclusionMap;
            if (glIsTextureHandleResidentARB(matl.occlusionMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(matl.occlusionMap);
            }
        }
        matl.albedo = albedo;
        matl.metallic = metallic;
        matl.roughness = roughness;
        matl.emissive = emissive;
        return matl;
    }
#else
    MaterialBindless::GpuData MaterialBindless::buildGpuData()
    {
        GpuData gpuData = { };
        if (albedoMap != nullptr) 
        {
            gpuData.albedoMap = albedoMap->gfxTexture->glHandle;
            gpuData.flag |= (u32)Flags::kHasAlbedoMap;
            if (glIsTextureHandleResidentARB(gpuData.albedoMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(gpuData.albedoMap);
            }
        }
        if (normalMap != nullptr)
        {
            gpuData.normalMap = normalMap->gfxTexture->glHandle;
            gpuData.flag |= (u32)Flags::kHasNormalMap;
            if (glIsTextureHandleResidentARB(gpuData.normalMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(gpuData.normalMap);
            }
        }
        if (metallicRoughnessMap != nullptr) 
        {
            gpuData.metallicRoughnessMap = metallicRoughnessMap->gfxTexture->glHandle;
            gpuData.flag |= (u32)Flags::kHasMetallicRoughnessMap;
            if (glIsTextureHandleResidentARB(gpuData.metallicRoughnessMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(gpuData.metallicRoughnessMap);
            }
        }
        if (occlusionMap != nullptr) 
        {
            gpuData.occlusionMap = occlusionMap->gfxTexture->glHandle;
            gpuData.flag |= (u32)Flags::kHasOcclusionMap;
            if (glIsTextureHandleResidentARB(gpuData.occlusionMap) == GL_FALSE) 
            {
                glMakeTextureHandleResidentARB(gpuData.occlusionMap);
            }
        }
        gpuData.albedo = albedo;
        gpuData.metallic = metallic;
        gpuData.roughness = roughness;
        gpuData.emissive = emissive;
        return gpuData;
    }
#endif
}