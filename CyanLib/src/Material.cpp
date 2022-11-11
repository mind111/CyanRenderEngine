#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"
#include "AssetManager.h"

namespace Cyan
{
    IMaterial::MaterialShaderMap IMaterial::materialShaderMap = {
        { TO_STRING(ConstantColorMatl), "PBRShader" },
        { TO_STRING(PBRMaterial), "PBRShader" },
        { TO_STRING(LightmappedPBRMatl), "LightmappedPBRShader" }
    };

    std::string Material<ConstantColor>::typeDesc = std::string(TO_STRING(ConstantColorMatl));
    Shader* ConstantColorMatl::shader = nullptr;

    std::string Material<PBR>::typeDesc = std::string(TO_STRING(PBRMaterial));
    Shader* PBRMaterial::shader = nullptr;

    std::string Material<Lightmapped<PBR>>::typeDesc = std::string(TO_STRING(LightmappedPBRMatl));
    Shader* LightmappedPBRMatl::shader = nullptr;

    std::string Material<Emissive<PBR>>::typeDesc = std::string(TO_STRING(EmissivePBRMatl));
    Shader* EmissivePBRMatl::shader = nullptr;

    void PBR::renderUI()
    {
        auto renderTextureCombo = [](const char* label, ITextureRenderable* previewTexture, const std::vector<ITextureRenderable*>& textures)
        {
            ImGui::Text(label + 2); ImGui::SameLine();
            const char* preview = "";
            if (previewTexture)
            {
                preview = previewTexture->name;
            }
            if (ImGui::BeginCombo(label, preview))
            {
                i32 selectedIndex = 0u;
                for (int n = 0; n < textures.size(); n++)
                {
                    const bool isSelected = (selectedIndex == n);
                    if (ImGui::Selectable(textures[n]->name, isSelected))
                    {
                        selectedIndex = n;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
                return selectedIndex;
            }
            return -1;
        };

        auto editTexture = [&renderTextureCombo](const char* label, Texture2DRenderable** outTexture, const std::vector<ITextureRenderable*>& textures) {
            i32 selectedTextureIndex = renderTextureCombo(label, *outTexture, textures);
            if (selectedTextureIndex > 0)
            {
                *outTexture = dynamic_cast<Texture2DRenderable*>(textures[selectedTextureIndex]);
            }
        };

        std::vector<ITextureRenderable*> textures = AssetManager::getTextures();

        editTexture("##BaseColor", &albedo, textures);
        editTexture("##Normal", &normal, textures);
        editTexture("##Roughness", &roughness, textures);
        editTexture("##Metallic", &metallic, textures);
        editTexture("##MetallicRoughness", &metallicRoughness, textures);
        editTexture("##occlusion", &occlusion, textures);

        ImGui::Text("kRoughness"); ImGui::SameLine();
        ImGui::SliderFloat("##kRoughness", &kRoughness, 0.f, 1.f);
        ImGui::Text("kMetallic"); ImGui::SameLine();
        ImGui::SliderFloat("##kMetallic", &kMetallic, 0.f, 1.f);
        f32 albedo[3] = { kAlbedo.r, kAlbedo.g, kAlbedo.b };
        ImGui::ColorPicker3("kAlbedo", albedo);
        kAlbedo.r = albedo[0];
        kAlbedo.g = albedo[1];
        kAlbedo.b = albedo[2];
    }
}