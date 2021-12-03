#pragma once
#include "glm.hpp"
#include "SceneNode.h"
namespace Cyan
{
    struct LightMap
    {
        struct SceneNode* m_owner;
        struct Texture* m_texAltas;
    };

    // alignment
    struct LightMapTask
    {
        glm::vec4 worldPosition;
        glm::vec4 normal;
        glm::vec2 texCoord;
    };

    class LightMapManager
    {
    public:
        static void      renderMeshInstanceToLightMap(struct MeshInstance* meshInstance);
        static LightMap* createLightMapForMeshInstance(struct ::SceneNode* node);
    };
}