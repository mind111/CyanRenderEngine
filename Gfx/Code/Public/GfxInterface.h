#pragma once

/**
 * Note: This file includes all the interfaces exposed by the Gfx module
 */

#include <memory>

#include "Gfx.h"
#include "CameraViewInfo.h"
#include "GfxStaticMesh.h"

namespace Cyan
{
    struct StaticSubMeshInstance
    {
        // todo: material
        std::string staticMeshInstanceKey;
        GfxStaticSubMesh* subMesh = nullptr;
        glm::mat4 localToWorldMatrix = glm::mat4(1.f);
    };
}