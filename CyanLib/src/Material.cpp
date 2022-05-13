#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"

namespace Cyan
{
    std::string Material<ConstantColor>::typeDesc = std::string(TO_STRING(ConstantColorMatl));
    Shader* ConstantColorMatl::shader = nullptr;

    std::string Material<PBR>::typeDesc = std::string(TO_STRING(PBRMatl));
    Shader* PBRMatl::shader = nullptr;

    std::string Material<Lightmapped<PBR>>::typeDesc = std::string(TO_STRING(LightmappedPBRMatl));
    Shader* LightmappedPBRMatl::shader = nullptr;

    std::string Material<Emissive<PBR>>::typeDesc = std::string(TO_STRING(EmissivePBRMatl));
    Shader* EmissivePBRMatl::shader = nullptr;

    struct ShaderSource
    {
        std::string vs;
        std::string gs;
        std::string ps;
        std::string cs;
    };

    Shader* initMaterialShader(std::string materialTypeDesc)
    {
        std::unordered_map<std::string, std::string> shaderMap = { 
            { TO_STRING(PBRMatl), "PBRShader" },
            { TO_STRING(LightmappedPBRMatl), "PBRShader" },
            { TO_STRING(EmissivePBRMatl), "PBRShader" },
            { TO_STRING(ConstantColor), "ConstantColorShader" },
        };

        auto entry = shaderMap.find(materialTypeDesc);
        if (entry == shaderMap.end())
        {
            return nullptr;
        }
    }
}