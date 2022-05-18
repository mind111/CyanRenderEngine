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
}