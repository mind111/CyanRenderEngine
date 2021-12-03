#include "LightMap.h"
#include "Mesh.h"
#include "Texture.h"
#include "CyanAPI.h"
#include "CyanRenderer.h"

namespace Cyan
{
    void LightMapManager::renderMeshInstanceToLightMap(MeshInstance* meshInstance)
    {

    }

    LightMap* LightMapManager::createLightMapForMeshInstance(SceneNode* node)
    {
        MeshInstance* meshInstance = node->m_meshInstance;
        glm::mat4 model = node->getWorldTransform().toMatrix();

        auto ctx = getCurrentGfxCtx();
        ctx->setDepthControl(DepthControl::kDisable);
        ctx->setRenderTarget(m_lightMapRenderTarget, 0);
        ctx->setViewport({ 0, 0, m_lightMap.m_texAltas->m_width, m_lightMap.m_texAltas->m_height });
        ctx->setShader(m_lightMapShader);
        for (u32 sm = 0; sm < roomMesh->m_subMeshes.size(); ++sm)
        {
            auto ctx = getCurrentGfxCtx();
            auto renderer = Renderer::getSingletonPtr();
            ctx->setVertexArray(roomMesh->m_subMeshes[sm]->m_vertexArray);
            ctx->drawIndex(roomMesh->m_subMeshes[sm]->m_numIndices);
        }
        ctx->setDepthControl(DepthControl::kEnable);
        return nullptr;
    }
}