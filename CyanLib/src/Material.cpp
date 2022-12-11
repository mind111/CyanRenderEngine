#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan {
    void Material::renderUI() {

    }

    GpuMaterial Material::buildGpuMaterial() {
        GpuMaterial matl = { };
        if (albedoMap != nullptr) {
            matl.albedoMap = albedoMap->glHandle;
            matl.flag |= (u32)Flags::kHasAlbedoMap;
            if (glIsTextureHandleResidentARB(matl.albedoMap) == GL_FALSE) {
                glMakeTextureHandleResidentARB(matl.albedoMap);
            }
        }
        if (normalMap != nullptr) {
            matl.normalMap = normalMap->glHandle;
            matl.flag |= (u32)Flags::kHasNormalMap;
            if (glIsTextureHandleResidentARB(matl.normalMap) == GL_FALSE) {
                glMakeTextureHandleResidentARB(matl.normalMap);
            }
        }
        if (metallicRoughnessMap != nullptr) {
            matl.metallicRoughnessMap = metallicRoughnessMap->glHandle;
            matl.flag |= (u32)Flags::kHasMetallicRoughnessMap;
            if (glIsTextureHandleResidentARB(matl.metallicRoughnessMap) == GL_FALSE) {
                glMakeTextureHandleResidentARB(matl.metallicRoughnessMap);
            }
        }
        if (occlusionMap != nullptr) {
            matl.occlusionMap = occlusionMap->glHandle;
            matl.flag |= (u32)Flags::kHasOcclusionMap;
            if (glIsTextureHandleResidentARB(matl.occlusionMap) == GL_FALSE) {
                glMakeTextureHandleResidentARB(matl.occlusionMap);
            }
        }
        matl.albedo = albedo;
        matl.metallic = metallic;
        matl.roughness = roughness;
        matl.emissive = emissive;
        return matl;
    }
}