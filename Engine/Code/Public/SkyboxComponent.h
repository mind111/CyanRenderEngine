#pragma once

#include "Engine.h"
#include "SceneComponent.h"

namespace Cyan
{
    class Texture2D;
    class Skybox;
    class GHTextureCube;

    class ENGINE_API SkyboxComponent : public SceneComponent
    {
    public:
        SkyboxComponent(const char* name, const Transform& localTransform);
        ~SkyboxComponent();

        virtual void setOwner(Entity* owner) override;

        void setHDRI(Texture2D* HDRI);
        GHTextureCube* getSkyboxCubemap();

    private:
        Texture2D* m_HDRI = nullptr;
        std::unique_ptr<Skybox> m_skybox = nullptr;
    };
}
