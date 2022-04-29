#include "Common.h"
#include "CyanAPI.h"
#include "Material.h"

namespace Cyan
{
    std::string Material<PBR>::typeName = std::string(TO_STRING(PBRMatl));
    Shader* PBRMatl::shader = nullptr;

    std::string Material<Lightmapped<PBR>>::typeName = std::string(TO_STRING(LightmappedPBRMatl));
    Shader* LightmappedPBRMatl::shader = nullptr;

    std::string Material<Emissive<PBR>>::typeName = std::string(TO_STRING(EmissivePBRMatl));
    Shader* EmissivePBRMatl::shader = nullptr;
}