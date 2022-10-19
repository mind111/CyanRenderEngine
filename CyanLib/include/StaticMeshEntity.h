#pragma once

#include "Mesh.h"
#include "Entity.h"
#include "SceneNode.h"

namespace Cyan
{
    struct Scene;
    struct IMaterial;

    struct StaticMeshEntity : public Entity
    {
        /* Entity interface */
        virtual void update() override { }
        virtual const char* getTypeDesc() override { return "StaticMeshEntity"; }
        virtual void renderUI() override;

        StaticMeshEntity(
            Scene* scene,
            const char* inName,
            const Transform& t,
            Mesh* inMesh,
            Entity* inParent = nullptr, 
            u32 inProperties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        ~StaticMeshEntity() { }

        MeshInstance* getMeshInstance() { return meshComponentPtr->meshInst; }

        void setMaterial(IMaterial* material);
        void setMaterial(IMaterial* material, u32 index);
    private:
        std::unique_ptr<MeshComponent> meshComponentPtr = nullptr;
    };
}
