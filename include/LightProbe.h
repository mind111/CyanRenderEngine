#pragma once

#include "Texture.h"

struct LightProbe
{
    Cyan::Texture* m_baseCubeMap;
    Cyan::Texture* m_diffuse;
    Cyan::Texture* m_specular;
    Cyan::Texture* m_brdfIntegral;
};
