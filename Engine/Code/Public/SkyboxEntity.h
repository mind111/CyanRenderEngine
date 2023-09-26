#pragma once

#include "Entity.h"
#include "Skybox.h"

namespace Cyan
{
    class SkyboxComponent;
    class Texture2D;

    class ENGINE_API SkyboxEntity : public Entity
    {
    public:
        SkyboxEntity(World* world, const char* name, const Transform& entityLocalTransform);
        virtual ~SkyboxEntity();

        SkyboxComponent* getSkyboxComponent() { return m_skyboxComponent.get(); }
    private:
        std::shared_ptr<SkyboxComponent> m_skyboxComponent = nullptr;
    };
}
